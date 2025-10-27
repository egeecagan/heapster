
#include "heapster.h"
#include "internal.h"
#include "internal_f.h"
#include <stdbool.h>
#include <string.h>

/*
main function definitions of public api
*/

static heapster_policy_t current_policy = HEAPSTER_FIRST_FIT;
static pthread_mutex_t policy_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t default_arena_size = 128 * 1024; // 128 KB 

void heapster_set_policy(heapster_policy_t policy) {
    pthread_mutex_lock(&policy_lock);
    current_policy = policy;
    pthread_mutex_unlock(&policy_lock);
}

heapster_policy_t heapster_get_policy(void) {
    pthread_mutex_lock(&policy_lock);
    heapster_policy_t policy = current_policy;
    pthread_mutex_unlock(&policy_lock);
    return policy;
}

int heapster_init(size_t arena_size, heapster_policy_t policy) {
    size_t min_size = ARENA_MIN_SIZE;

    if (arena_size < min_size) {
        fprintf(stderr, "[heapster] arena_size too small, using minimum %zu\n", min_size);
        arena_size = min_size;
    }

    default_arena_size = arena_size;
    heapster_set_policy(policy);

    arena_t *arena = arena_create(default_arena_size);
    return arena ? 0 : -1;
}

int heapster_finalize(void) {
    pthread_mutex_lock(&arena_list_lock);

    last_cleanup();

    pthread_mutex_unlock(&arena_list_lock);

    pthread_mutex_destroy(&policy_lock);
    pthread_mutex_destroy(&arena_list_lock);

    return 0;
}

// requested size bu parametre iste kullanicinin block payloadinda bu kadar yer olmasu lazim minimum
void *heapster_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    // İstenen payload boyutunu hizala
    size_t aligned_payload_size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);

    arena_t *arena = arena_get_list();
    block_header_t *block = NULL;
    arena_t *found_arena = NULL;

    // 1. Mevcut Arenalarda Uygun Blok Bul
    while (arena) {
        // arena_find_free_block zaten ilgili arena kilidini kullanır ve açar.
        block = arena_find_free_block(arena, aligned_payload_size);
        if (block) {
            found_arena = arena;
            break;
        }
        arena = arena->next;
    }

    // 2. Blok Bulunamadıysa Yeni Arena Oluştur
    if (!block) {
        // Yeni arena için gereken tahmini boyutu hesapla
        size_t arena_size = aligned_payload_size + BLOCK_HEADER_SIZE + ARENA_HEADER_SIZE;

        if (arena_size < ARENA_MIN_SIZE) {
            arena_size = ARENA_MIN_SIZE;
        }

        arena_t *new_arena = arena_create(arena_size);
        if (!new_arena) {
            return NULL; 
        }

        found_arena = new_arena;
        
        // Yeni arenada bloğu tekrar bul (mmap/sbrk'den gelen tek serbest blok)
        block = arena_find_free_block(new_arena, aligned_payload_size);
    }
    
    // Hala blok yoksa (hata durumu)
    if (!block) {
        return NULL;
    }

    arena = found_arena;
    
    // Kilit altına al ve İstatistik Güncellemelerini yap
    pthread_mutex_lock(&arena->lock);
    
    void *payload_ptr = NULL;
    block_header_t *allocated_block = NULL;

    // ** İSTATİSTİK: ÇAĞRI SAYISI **
    arena->stats.malloc_calls++;

    // 3. Bölme (Split) Kontrolü
    if (block->size >= aligned_payload_size + BLOCK_MIN_SIZE) {
        
        // Split işlemi deneniyor
        block_header_t *splitted_first_part = block_split(arena, block, aligned_payload_size);
        
        // Split başarıyla sonuçlandıysa
        if (splitted_first_part) { 
            allocated_block = splitted_first_part;
            
            // Tahsis edilen bloğu ayarla
            allocated_block->requested_size = size;
            payload_ptr = block_to_payload(allocated_block);
            
            // ** İSTATİSTİK GÜNCELLEME (SPLIT) **
            size_t allocated_size = allocated_block->size;
            
            arena->stats.used_bytes += allocated_size; 
            arena->stats.free_bytes -= allocated_size; 
            
            arena->stats.wasted_bytes += (allocated_size - size);
            arena->stats.allocated_block_count++;
            // free_block_count, block_split içinde zaten artırıldı.
            
        } else {
            // Split başarısız (bu koşulda nadir olmalı, ama ihtimal dahilinde)
            // Bu durumda payload_ptr NULL kalır, aşağıdaki unlock'tan sonra NULL döner.
        }

    } else {
        // 4. Tam Kullanım Durumu (Split Yok)
        
        // Bloğu serbest listeden çıkar (Tamamen kullanıldığı için)
        block_remove_from_free_list(arena, block);

        // Bloğu tahsis edilmiş olarak işaretle
        block->free = 0;
        block->requested_size = size;    
        block->magic = CTRL_CHR; 
        payload_ptr = block_to_payload(block);
        allocated_block = block; // Tahsis edilen blok bu.
        
        // ** İSTATİSTİK GÜNCELLEME (TAM KULLANIM) **
        size_t allocated_size = allocated_block->size;

        arena->stats.used_bytes += allocated_size; 
        arena->stats.free_bytes -= allocated_size; 
        
        arena->stats.wasted_bytes += (allocated_size - size);
        
        arena->stats.allocated_block_count++;
        arena->stats.free_block_count--; // Serbest blok tamamen kullanıldığı için azalır.
    }
    
    pthread_mutex_unlock(&arena->lock);
    
    return payload_ptr;
}

// n member and each member has the size of size
void *heapster_calloc(size_t nmemb, size_t size) {

    // 1. Taşma (Overflow) Kontrolü
    size_t total = nmemb * size;
    // nmemb * size'ın taşma yapıp yapmadığını kontrol eder.
    if (size != 0 && total / size != nmemb) {
        fprintf(stderr, "[heapster] calloc: multiplication overflow detected.\n");
        return NULL;
    }

    // 2. Bellek Tahsisi
    void *ptr = heapster_malloc(total); 
    if (!ptr) {
        return NULL;
    }

    // 3. İstatistik Güncelleme (malloc -> calloc)
    block_header_t *block = payload_to_block(ptr);
    arena_t *arena = arena_get_list(); 
    
    // İlgili arenayı bul
    while (arena && arena->id != block->arena_id) {
        arena = arena->next;
    }
    
    if (arena) {
        pthread_mutex_lock(&arena->lock); 
        
        // heapster_malloc içinde sayılan çağrıyı (malloc_calls) geri al,
        // yerine calloc_calls'u say.
        if (arena->stats.malloc_calls > 0) {
            arena->stats.malloc_calls--;
        }
        arena->stats.calloc_calls++;
        
        pthread_mutex_unlock(&arena->lock);
    }
    
    // 4. Belleği Sıfırlama
    memset(ptr, 0, total); 
    return ptr;
}

void *heapster_realloc(void *ptr, size_t size) {

    // 1. Edge Case: ptr NULL ise, malloc çağrısı yapılır.
    if (!ptr) {
        return heapster_malloc(size); 
    }
    
    // 2. Edge Case: size 0 ise, free çağrısı yapılır.
    if (size == 0) {
        heapster_free(ptr);
        return NULL;
    }

    block_header_t *block = payload_to_block(ptr);

    // Blok doğrulama kontrolü
    if (block_validate(block) <= 0) {
        fprintf(stderr, "[heapster] invalid realloc %p\n", ptr);
        return NULL;
    }

    // İlgili arenayı bul
    arena_t *arena = arena_get_list();
    while (arena && arena->id != block->arena_id) {
        arena = arena->next;
    }

    // Arena bulunamazsa (hata durumu)
    if (!arena) {
        fprintf(stderr, "[heapster] realloc: block %p arena not found\n", ptr);
        return NULL;
    }

    // ** İSTATİSTİK: ÇAĞRI SAYISI **
    pthread_mutex_lock(&arena->lock); 
    arena->stats.realloc_calls++;
    pthread_mutex_unlock(&arena->lock);
    // ** ÇAĞRI SAYISI GÜNCEL **
    
    size_t aligned_payload_size = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    
    // 3. Yerinde Yeniden Boyutlandırma (In-Place Resize)
    // Yeni istenen boyut, mevcut bloğun payload boyutundan küçük veya eşitse
    if (block->size >= aligned_payload_size) {
        
        // Split/Daraltma İçin Kilit Al
        pthread_mutex_lock(&arena->lock);
        
        // Eğer küçültme, minimum blok boyutunun üzerinde bir serbest parça bırakıyorsa:
        if (block->size >= aligned_payload_size + BLOCK_MIN_SIZE) {
            
            // İstatistik Güncelleme İçin Eski Değerler
            size_t old_block_size = block->size;
            size_t old_requested_size = block->requested_size; 
            
            // block_split, bloğu ikiye ayırır ve serbest kalan parçayı free_list'e ekler.
            block_header_t *remaining_free_block = block_split(arena, block, aligned_payload_size);
            
            // Eğer block_split başarılı olduysa (ki koşullar sağlandığı için olmalı)
            if (remaining_free_block) {
                
                // İstatistik Güncelleme
                size_t new_block_size = block->size; // Yeni (küçülmüş) payload boyutu
                
                // used_bytes: Küçülen miktar used'dan düşer.
                arena->stats.used_bytes -= (old_block_size - new_block_size); 
                
                // free_bytes: Serbest kalan parça kadar artar (remaining_free_block->size)
                arena->stats.free_bytes += remaining_free_block->size;
                
                // wasted_bytes: Eski israfı düş, yeni israfı ekle (daraltma sonrası)
                arena->stats.wasted_bytes -= (old_block_size - old_requested_size); // eski israfı çıkar
                arena->stats.wasted_bytes += (new_block_size - size);              // yeni israfı ekle
                
                // largest_free_block, free_block_count: block_split içinde güncellenmeli (varsayım)
            }
        } else {
            // Daralma var ama kalan kısım çok küçük, split yapılamaz (in-place daraltma)
            // Sadece wasted_bytes güncellenmeli. block->size aynı kalır.
            pthread_mutex_lock(&arena->lock); // Zaten yukarıda lock var, bu kısım gereksiz.
            
            size_t old_requested_size = block->requested_size;
            arena->stats.wasted_bytes -= (block->size - old_requested_size); // eski israfı çıkar
            arena->stats.wasted_bytes += (block->size - size);                  // yeni israfı ekle
            
            // pthread_mutex_unlock(&arena->lock); // Gereksiz.
        }

        // requested_size'ı güncelle ve pointer'ı döndür
        block->requested_size = size;
        pthread_mutex_unlock(&arena->lock);
        return ptr;
    }

    // 4. Yeni Tahsis ve Kopyalama (Boyut Yetersiz)
    
    void *new_ptr = heapster_malloc(size); // Malloc istatistikleri günceller
    if (!new_ptr) {
        return NULL;
    }

    // Kopyalanacak boyutu belirle
    size_t old_used = block->requested_size; 
    size_t copy_n = old_used < size ? old_used : size;

    // Veriyi kopyala
    memcpy(new_ptr, ptr, copy_n);

    // Eski bloğu serbest bırak
    heapster_free(ptr); // Free istatistikleri günceller

    return new_ptr;
}

/* 
arena mmap ile alindi ve tek blok var bu coalesce sonrasi arenayi sil
tek block yok 2 3 tane var silme

sbrk ile alinmis arena ama en sondaki ise sil tamamen
sbrk ile alinmis ama ortadakilerden biri o zaman silme sadece reset
*/

void heapster_free(void *ptr) {
    if (!ptr) return;

    block_header_t *block = payload_to_block(ptr);
    
    // 1. Blok Doğrulama
    if (block_validate(block) <= 0) {
        fprintf(stderr, "[heapster] invalid free %p\n", ptr);
        return;
    }

    // 2. Arena'yı Bulma
    arena_t *arena = arena_get_list();
    while (arena) {
        if (arena->id == block->arena_id) {
            
            pthread_mutex_lock(&arena->lock);

            // *********************************************************
            // 3. İSTATİSTİK GÜNCELLEMELERİ (FREE)
            // *********************************************************
            size_t freed_payload_size = block->size;
            
            arena->stats.free_calls++;
            arena->stats.used_bytes -= freed_payload_size;
            arena->stats.allocated_block_count--;
            
            // Serbest kalan payload alanını free_bytes'a ekle
            arena->stats.free_bytes += freed_payload_size; 
            
            // İç Fragmentasyon İadesi: Daha önce atanan israfı geri al
            arena->stats.wasted_bytes -= (freed_payload_size - block->requested_size);
            
            // Yeni bir serbest blok oluşacağı için sayacı artır (birleşme sonradan azaltacak)
            arena->stats.free_block_count++;
            // *********************************************************

            block->free = 1;
            block->requested_size = 0;

            // 4. Birleştirme (Coalescing)
            block_header_t *coalesced_block = block_coalesce(arena, block);
            
            // Birleşme sonrası en büyük serbest bloğu güncelle
            if (coalesced_block && coalesced_block->size > arena->stats.largest_free_block) {
                arena->stats.largest_free_block = coalesced_block->size;
            }
            
            // 5. Arena Yok Etme Kontrolü
            bool destroy = false;
            // Tüm bellek tek bir serbest bloksa ve tahsis edilen bellek kalmadıysa (sadece mmap için daha önemlidir)
            if (arena->block_count == 1 &&
                arena->free_list_head == coalesced_block &&
                !coalesced_block->phys_prev &&
                !coalesced_block->phys_next &&
                coalesced_block->size + BLOCK_HEADER_SIZE ==
                   arena->size - ARENA_HEADER_SIZE) {
                destroy = true;
            }

            pthread_mutex_unlock(&arena->lock);

            if (destroy) {
                arena_destroy(arena);  
            }
            return;
        }
        arena = arena->next;
    }

    fprintf(stderr, "[heapster] free: block %p not found in any arena\n", ptr);
}


