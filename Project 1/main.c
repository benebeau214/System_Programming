#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "list.h"
#include "hash.h"
#include "bitmap.h"

#define MAX 10  //최대 10개개

/* 리스트 저장 공간 */
struct list lists[MAX];
char list_names[MAX][20];  // 리스트 이름 저장
int list_count = 0;  // 현재 리스트 개수

//해쉬 저장 공간
struct hash hashes[MAX];
char hash_names[MAX][20];
int hash_count = 0; 

struct bitmap *bitmaps[MAX]; 
char bitmap_names[MAX][20];
int bitmap_cnt = 0;

/* 리스트 정렬을 위한 비교 함수 (오름차순 정렬) */
bool list_less_than(const struct list_elem *a, const struct list_elem *b, void *aux) {
    struct list_item *item_a = list_entry(a, struct list_item, elem);
    struct list_item *item_b = list_entry(b, struct list_item, elem);
    return item_a->data < item_b->data;  // 오름차순 정렬 (작은 값이 앞으로)
}

// 해시 비교 함수
bool hash_less_than(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
    struct hash_item *item_a = hash_entry(a, struct hash_item, elem);
    struct hash_item *item_b = hash_entry(b, struct hash_item, elem);
    return item_a->data < item_b->data;
}

// 해시 함수 (정수 해시)
unsigned hash_hash_function(const struct hash_elem *e, void *aux) {
    struct hash_item *item = hash_entry(e, struct hash_item, elem);
    return hash_int(item->data);
}

//해시 action 함수들
void hash_action_square(struct hash_elem *e, void *aux) {
    struct hash_item *item = hash_entry(e, struct hash_item, elem);
    item->data  *= item-> data;
}

void hash_action_triple(struct hash_elem *e, void *aux) {
    struct hash_item *item = hash_entry(e, struct hash_item, elem);
    item->data  = item->data * item->data * item->data;
}

void hash_action_destructor(struct hash_elem *e, void *aux) {
    struct hash_item *item = hash_entry(e, struct hash_item, elem);
    
    if (item != NULL) {
        free(item);  // 메모리 해제
    }
}


/* 명령어 처리 함수 */
void process_command(char *command) {
    char name[20], src_name[20], type[20]; 
    int value, index, i, index1, index2, first_index, last_index, count, start;
    char val_str[6];
    bool val;

    // create 동작 
    if (sscanf(command, "create %s %s %d", type, name, &value) >= 2) {
        if (strcmp(type, "list") == 0) {
            if (list_count >= MAX) {
                printf("Error: Maximum number of lists reached.\n");
                return;
            }
            list_init(&lists[list_count]);  
            strcpy(list_names[list_count], name);  // 리스트 이름 저장
            list_count++;
        } 
        else if (strcmp(type, "hashtable") == 0) {
            if (hash_count >= MAX) {
                printf("Error: Maximum number of hash tables reached.\n");
                return;
            }
            if (hash_init(&hashes[hash_count], hash_hash_function, hash_less_than, NULL)) {
                strcpy(hash_names[hash_count], name);
                hash_count++;
            } else {
                printf("Error: Failed to initialize hashtable.\n");
            }
        }else if(strcmp(type, "bitmap") == 0){
            if(bitmap_cnt >= MAX){
                return;
            }
            bitmaps[bitmap_cnt] = bitmap_create(value);
            strcpy(bitmap_names[bitmap_cnt], name);
            bitmap_cnt++;
        }
    }

    // 삭제 동작 
    else if (sscanf(command, "delete %s", name) == 1) {
        int found = 0;  // 삭제 여부 확인
    
        // 리스트인지 확인 후 삭제
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                list_init(&lists[i]);  // 리스트 초기화
                list_names[i][0] = '\0';  // 리스트 이름 제거
                found = 1;
                break;
            }
        }
        // 해시 테이블인지 확인 후 삭제 (리스트에서 찾지 못했을 경우)
        if (!found) {
            for (i = 0; i < hash_count; i++) {
                if (strcmp(hash_names[i], name) == 0) {
                    hash_destroy(&hashes[i], NULL);
    
                    // 해시 테이블 목록 정리
                    for (int j = i; j < hash_count - 1; j++) {
                        hashes[j] = hashes[j + 1];
                        strcpy(hash_names[j], hash_names[j + 1]);
                    }
                    hash_count--;
    
                    found = 1;
                    break;
                }
            }
        }
        
        if(!found){
            for (i = 0; i < bitmap_cnt; i++) {
                if (strcmp(bitmap_names[i], name) == 0) {
                    bitmap_destroy(bitmaps[i]);

                    for (int j = i; j < bitmap_cnt - 1; j++) {
                        bitmaps[j] = bitmaps[j + 1];
                        strcpy(bitmap_names[j], bitmap_names[j + 1]);
                    }
                    bitmap_cnt--;
                    found = 1;
                    break;
                }
            }
        }
    }

     // 리스트에 값 추가 (맨 앞) 
     else if (sscanf(command, "list_push_front %s %d", name, &value) == 2) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                struct list_item *item = malloc(sizeof(struct list_item));
                item->data = value;
                list_push_front(&lists[i], &item->elem);  // list_elem을 전달
                return;
            }
        }
    }

    /* 리스트에 값 추가 (맨 뒤) */
    else if (sscanf(command, "list_push_back %s %d", name, &value) == 2) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                struct list_item *item = malloc(sizeof(struct list_item));
                item->data = value;
                list_push_back(&lists[i], &item->elem);  // list_elem을 전달
                return;
            }
        }
    }

    /* 리스트에서 값 제거 (맨 앞) */
    else if (sscanf(command, "list_pop_front %s", name) == 1) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                list_pop_front(&lists[i]);
                return;
            }
        }
    }

    /* 리스트에서 값 제거 (맨 뒤) */
    else if (sscanf(command, "list_pop_back %s", name) == 1) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                list_pop_back(&lists[i]);
                return;
            }
        }
    }

     /* 데이터 출력 */
     else if (sscanf(command, "dumpdata %s", name) == 1) {
        int found = 0;  // 해당하는 데이터 구조를 찾았는지 확인
    
        // 리스트인지 확인 후 출력
        for (int i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                struct list_elem *e;
                for (e = list_begin(&lists[i]); e != list_end(&lists[i]); e = list_next(e)) {
                    struct list_item *item = list_entry(e, struct list_item, elem);
                    printf("%d ", item->data);
                }
                printf("\n");  // 줄바꿈 추가
                fflush(stdout);  // 출력 즉시 반영
                found = 1;
                break;
            }
        }
    
        // 해시 테이블인지 확인 후 출력
        if (!found) {
            for (int i = 0; i < hash_count; i++) {
                if (strcmp(hash_names[i], name) == 0) {                
                    struct hash_iterator it;
                    hash_first(&it, &hashes[i]);
    
                    while (hash_next(&it)) {
                        struct hash_item *item = hash_entry(hash_cur(&it), struct hash_item, elem);
                        printf("%d ", item->data);
                    }
                    printf("\n");  // 줄바꿈 추가
                    fflush(stdout);  // 출력 즉시 반영
                    found = 1;
                    break;
                }
            }
        }
    
        // 비트맵인지 확인 후 출력
        if (!found) {
            for (int i = 0; i < bitmap_cnt; i++) {
                if (strcmp(bitmap_names[i], name) == 0) {
                    for (size_t j = 0; j < bitmap_size(bitmaps[i]); j++) {
                        printf("%d", bitmap_test(bitmaps[i], j));  // 0 또는 1 출력
                    }
                    printf("\n");  // 줄바꿈 추가
                    fflush(stdout);  // 출력 즉시 반영
                    found = 1;
                    break;
                }
            }
        }
    }
    
    else if (sscanf(command, "list_remove %s %d", name, &index) == 2) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                if (list_empty(&lists[i])) {
                    printf("Error: List is empty.\n");
                    return;
                }
            
                struct list_elem *e = list_begin(&lists[i]);
                for (int i = 0; i < index; i++) {
                    if (e == list_end(&lists[i])) {
                        printf("Error: Index out of bounds.\n");
                        return;
                    }
                    e = list_next(e);
                }
            
                if (e != list_end(&lists[i])) {  // 요소가 유효한 경우에만 제거
                    struct list_item *item = list_entry(e, struct list_item, elem);  
                    list_remove(e);  // 리스트에서 제거
                    free(item);  // 메모리 해제
                } else {
                    printf("Error: Invalid index.\n");
                }
                return;
            }
        }
    }
    // 리스트 맨 앞 데이터 출력
    else if (sscanf(command, "list_front %s", name) == 1) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                if (list_empty(&lists[i])) { //빈 리스트인지 확인
                    printf("Error: List %s is empty.\n", name);
                    return;
                }
                struct list_elem *front_elem = list_front(&lists[i]);  // 리스트의 첫 번째 요소 가져오기
                struct list_item *front_item = list_entry(front_elem, struct list_item, elem);  // 변환
                printf("%d\n", front_item->data);  // 정수 값 출력
                return;
            }
        }
    }
   // 리스트 맨 뒤 데이터 출력
    else if (sscanf(command, "list_back %s", name) == 1) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                if (list_empty(&lists[i])) { //빈 리스트인지 확인
                    printf("Error: List %s is empty.\n", name);
                    return;
                }
                struct list_elem *back_elem = list_back(&lists[i]);  // 리스트의 마지막 요소 가져오기
                struct list_item *back_item = list_entry(back_elem, struct list_item, elem);  // 변환
                printf("%d\n", back_item->data);  // 정수 값 출력
                return;
            }
        }
    }
    // 리스트에 지정 값 삽입입
    else if (sscanf(command, "list_insert %s %d %d", name, &index, &value) == 3) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                /*if (index < 0) {
                    printf("Error: Invalid index.\n");
                    return;
                }*/
            
                struct list_elem *e = list_begin(&lists[i]);
                for (int i = 0; i < index; i++) {
                    if (e == list_end(&lists[i])) {
                        printf("Error: Index out of bounds.\n");
                        return;
                    }
                    e = list_next(e);
                }
            
                struct list_item *item = malloc(sizeof(struct list_item));
            
                item->data = value;
                list_insert(e, &item->elem);  // `before` 요소 앞에 삽입
                return;
            }
        }
    }
    //비어있는 리스트인지 판별별
    else if (sscanf(command, "list_empty %s", name) == 1) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                if (list_empty(&lists[i])) {
                    printf("true\n");  // 리스트가 비어 있으면 true 출력
                } else {
                    printf("false\n");  // 리스트가 비어 있지 않으면 false 출력
                }
                return;
            }
        }
    }

    // 리스트에서 제일 큰 값 구하기
    else if (sscanf(command, "list_max %s", name) == 1) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                if (list_empty(&lists[i])) {
                    printf("Error: List %s is empty.\n", name);
                    return;
                }
                struct list_elem *max_elem = list_max(&lists[i], list_less_than, NULL);
                struct list_item *max_item = list_entry(max_elem, struct list_item, elem);
                printf("%d\n", max_item->data);
                return;
            }
        }
    }
    //리스트에서 제일 작은 값 구하기
    else if (sscanf(command, "list_min %s", name) == 1) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                if (list_empty(&lists[i])) {
                    printf("Error: List %s is empty.\n", name);
                    return;
                }
                struct list_elem *min_elem = list_min(&lists[i], list_less_than, NULL);
                struct list_item *min_item = list_entry(min_elem, struct list_item, elem);
                printf("%d\n", min_item->data);
                return;
            }
        }
    }
    // 리스트 사이즈 구하기
    else if (sscanf(command, "list_size %s", name) == 1) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                printf("%zu\n", list_size(&lists[i]));  
                return;
            }
        }
    }
   // 정렬된 값들 사이에 추가적으로 값값 삽입
    else if (sscanf(command, "list_insert_ordered %s %d", name, &value) == 2) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                struct list_item *item = malloc(sizeof(struct list_item));
                item->data = value;
                list_insert_ordered(&lists[i], &item->elem, list_less_than, NULL);
                return;
            }
        }
    } 
    // 순서 거꾸로 만들기
    else if (sscanf(command, "list_reverse %s", name) == 1) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                list_reverse(&lists[i]);  // 리스트 뒤집기 실행
                return;
            }
        }
    }
    // 값 무작위로 섞기
    else if (sscanf(command, "list_shuffle %s", name) == 1) {
        for (int i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                list_shuffle(&lists[i]); // 무작위 섞기기
                return;
            }
        }        
    }
    //리스트 정렬
    else if (sscanf(command, "list_sort %s", name) == 1) {
        for (i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                list_sort(&lists[i], list_less_than, NULL);  // 리스트 정렬 실행
                return;
            }
        }
    }
    // 리스트에 있는 두 개의 요소 위치 바꾸기
    else if (sscanf(command, "list_swap %s %d %d", name, &index1, &index2) == 3) {
        struct list *target_list = NULL;
    
        // 대상 리스트 찾기
        for (int i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                target_list = &lists[i];
                break;
            }
        }
    
        if (target_list == NULL) {
            printf("Error: List %s not found.\n", name);
            return;
        }
    
        if (index1 == index2) {
            printf("Warning: Same index provided, no swap needed.\n");
            return;
        }
    
        // 두 개의 요소 위치 찾기
        struct list_elem *elem1 = list_begin(target_list);
        struct list_elem *elem2 = list_begin(target_list);
    
        for (int i = 0; i < index1 && elem1 != list_end(target_list); i++) {
            elem1 = list_next(elem1);
        }
    
        for (int i = 0; i < index2 && elem2 != list_end(target_list); i++) {
            elem2 = list_next(elem2);
        }
    
        if (elem1 == list_end(target_list) || elem2 == list_end(target_list)) {
            printf("Error: Invalid index.\n");
            return;
        }
    
        // 리스트 요소 스왑
        list_swap(elem1, elem2);
    }
    
    
    else if (sscanf(command, "list_unique %s %s", name, src_name) >= 1) {
        struct list *target_list = NULL, *duplicates_list = NULL;
    
        // 대상 리스트 찾기
        for (int i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                target_list = &lists[i];
            }
            if (sscanf(command, "list_unique %s %s", name, src_name) == 2 &&
                strcmp(list_names[i], src_name) == 0) {
                duplicates_list = &lists[i];
            }
        }
    
        if (target_list == NULL) {
            printf("Error: List %s not found.\n", name);
            return;
        }
    
        // 중복 제거 실행
        list_unique(target_list, duplicates_list, list_less_than, NULL);

    }
    
    else if (sscanf(command, "list_splice %s %d %s %d %d", name, &index, src_name, &first_index, &last_index) == 5) {
        struct list *dest_list = NULL, *src_list = NULL;
    
        // 대상 리스트 찾기
        for (int i = 0; i < list_count; i++) {
            if (strcmp(list_names[i], name) == 0) {
                dest_list = &lists[i];
            }
            if (strcmp(list_names[i], src_name) == 0) {
                src_list = &lists[i];
            }
        }
    
        if (dest_list == NULL || src_list == NULL) {
            printf("Error: One or both lists not found.\n");
            return;
        }
    
        if (index < 0 || first_index < 0 || last_index < first_index) {
            printf("Error: Invalid index values.\n");
            return;
        }
    
        // 대상 리스트에서 `index` 위치 찾기
        struct list_elem *before = list_begin(dest_list);
        for (int i = 0; i < index && before != list_end(dest_list); i++) {
            before = list_next(before);
        }
    
        // 옮길 요소 범위 찾기
        struct list_elem *first = list_begin(src_list);
        for (int i = 0; i < first_index && first != list_end(src_list); i++) {
            first = list_next(first);
        }
    
        struct list_elem *last = first;
        for (int i = first_index; i < last_index && last != list_end(src_list); i++) {
            last = list_next(last);
        }
        // 리스트 스플라이스 실행
        list_splice(before, first, last);
    
    } else if (sscanf(command, "hash_insert %s %d", name, &value) == 2) {
        for (i = 0; i < hash_count; i++) {
            if (strcmp(hash_names[i], name) == 0) {
                // 새로운 hash_item 할당 및 초기화
                struct hash_item *item = malloc(sizeof(struct hash_item));
    
                item->data = value;  // 데이터 값 저장
    
                // 중복 확인 후 삽입
                struct hash_elem *result = hash_insert(&hashes[i], &item->elem);
    
                if (result != NULL) {
                    // 중복된 경우 메모리 해제
                    free(item);
                } 
                return;
            }
        }
    } else if (sscanf(command, "hash_apply %s %s", name, type) == 2) {
        hash_action_func *action = NULL;
    
        // type에 따른 적절한 함수 선택
        if (strcmp(type, "square") == 0) {
            action = hash_action_square;
        } else if (strcmp(type, "triple") == 0) {
            action = hash_action_triple;
        } 
    
        // 해시 테이블 찾기 및 적용
        for (i = 0; i < hash_count; i++) {
            if (strcmp(hash_names[i], name) == 0) {
                hash_apply(&hashes[i], action);
                return;
            }
        }
    }
    // 해시 테이블 내 특정 값 삭제
    else if (sscanf(command, "hash_delete %s %d", name, &value) == 2) {
        for (i = 0; i < hash_count; i++) {
            if (strcmp(hash_names[i], name) == 0) {

                //임시 저장 변수
                struct hash_item temp_item;
                temp_item.data = value;

                struct hash_elem *e = hash_find(&hashes[i], &temp_item.elem);
                if(e != NULL){
                    hash_delete(&hashes[i], e);
                    free(hash_entry(e, struct hash_item, elem));
                }
                return;
            }
        }
    }
    // 해시 테이블 내 값 없애버리기
    else if (sscanf(command, "hash_clear %s", name) == 1) {
        for (i = 0; i < hash_count; i++) {
            if (strcmp(hash_names[i], name) == 0) {
                hash_clear(&hashes[i], hash_action_destructor);
                return;
            }
        }
    }
    //지정한 해시 테이블에 저장된 요소 개수를 출력
    else if (sscanf(command, "hash_size %s", name) == 1) {

        for (i = 0; i < hash_count; i++) {
            if (strcmp(hash_names[i], name) == 0) {
                printf("%zu", hash_size(&hashes[i]));
            }
            return;
         }
    }
    // 해시 테이블이 비어 있는지 확인하여 true/false 출력
    else if(sscanf(command, "hash_empty %s", name) == 1){
        for (i = 0; i < hash_count; i++) {
            if (strcmp(hash_names[i], name) == 0) {
                if(hash_empty(&hashes[i])){
                    printf("true\n");
                }
                else printf("false\n");
                return;
            }
        }
    }
    // 해시 테이블에서 특정 값을 가진 요소를 찾아 출력
    else if (sscanf(command, "hash_find %s %d", name, &value) == 2) {
        for (i = 0; i < hash_count; i++) {
            if (strcmp(hash_names[i], name) == 0) {
                // 찾고자 하는 요소를 임시로 생성
                struct hash_item temp_item;
                temp_item.data = value;
    
                struct hash_elem *e = hash_find(&hashes[i], &temp_item.elem);
                if (e != NULL) {
                    struct hash_item *found_item = hash_entry(e, struct hash_item, elem);
                    printf("%d\n", found_item->data);  // 실제 저장된 값 출력
                }
                return;  // 명령 실행 후 종료
            }
        }
    }
    // 동일한 키가 있으면 교체하고, 없으면 새로 삽입
    else if(sscanf(command, "hash_replace %s %d", name, &value) == 2){
        for (i = 0; i < hash_count; i++) {
            if (strcmp(hash_names[i], name) == 0) {
                
                struct hash_item *new_item = malloc(sizeof(struct hash_item));
                new_item->data = value;

                struct hash_elem *old_elem = hash_replace(&hashes[i], &new_item->elem);

                if (old_elem != NULL) {
                    struct hash_item *old_item = hash_entry(old_elem, struct hash_item, elem);
                    free(old_item);
                }
                
            }
        }
    }
    // 지정한 인덱스의 비트를 true(1)로 설정
    else if(sscanf(command, "bitmap_mark %s %d", name, &index) == 2){
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                     bitmap_mark(bitmaps[i], index);
                     return;
                }
                
            }
    }
    // 지정한 범위의 모든 비트가 true인지 검사
    else if(sscanf(command, "bitmap_all %s %d %d", name, &start, &count) == 3){
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {

                    if(bitmap_all(bitmaps[i], start, count)){
                        printf("true\n");
                        return;
                    }else printf("false\n");
                    return;
                }
            }
    }
    // 지정한 범위에 true(1) 비트가 하나라도 있는지 검사
    else if(sscanf(command, "bitmap_any %s %d %d", name, &start, &count) == 3){
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                    if(bitmap_any(bitmaps[i],start, count)){
                        printf("true\n");
                        return;
                    }else printf("false\n");
                    return;
                }
                
            }
    }
    // 지정한 범위가 모두 특정 값(true/false)과 일치하는지 검사
    else if(sscanf(command, "bitmap_contains %s %d %d %5s", name, &start, &count, val_str) == 4){

        if(strcmp(val_str, "true") == 0){
            val = true;
        } else if (strcmp(val_str, "false") == 0){
            val = false; 
        }
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                    if(bitmap_contains(bitmaps[i], start, count, val)){
                        printf("true\n");
                        return;
                    }else printf("false\n");
                    return;
            }
        }
    }
    // 지정한 범위에서 true 또는 false인 비트 개수 출력
    else if (sscanf(command, "bitmap_count %s %d %d %5s", name, &start, &count, val_str) == 4) {
        if (strcmp(val_str, "true") == 0) {
            val = true;
        } else if (strcmp(val_str, "false") == 0) {
            val = false;
        }
    
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                printf("%zu\n", bitmap_count(bitmaps[i], start, count, val));  
                fflush(stdout);  
                return;
            }
        }
    }
    // 비트맵의 전체 비트 상태를 16진수로 출력
    else if(sscanf(command, "bitmap_dump %s", name) == 1){
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                bitmap_dump(bitmaps[i]); //16비트로 변환
                fflush(stdout);  
                return;
            }
        }
    }
    // 지정한 범위에 true 비트가 하나도 없는지 검사
    else if(sscanf(command, "bitmap_none %s %d %d", name, &start, &count) == 3){
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {

                    if(bitmap_none(bitmaps[i],start, count)){
                        printf("true\n");
                        return;
                    }else printf("false\n");
                    return;
                }
            }
    }
    // 지정한 인덱스의 비트를 반전(true ↔ false)
    else if(sscanf(command, "bitmap_flip %s %d", name, &index) == 2){
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                    bitmap_flip(bitmaps[i],index);
                }
            }
    }
    // 지정한 인덱스의 비트를 false(0)로 설정
    else if(sscanf(command, "bitmap_reset %s %d", name, &index) == 2){
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                    bitmap_reset(bitmaps[i],index);
                }
            }
    }
    // 지정한 범위에서 연속된 특정 값의 비트를 검색하여 시작 인덱스 반환
    else if (sscanf(command, "bitmap_scan %s %d %d %5s", name, &start, &count, val_str) == 4) {
        if (strcmp(val_str, "true") == 0) {
            val = true;
        } else if (strcmp(val_str, "false") == 0) {
            val = false;
        }
    
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                printf("%zu\n", bitmap_scan(bitmaps[i], start, count, val));  
                fflush(stdout);  
                return;
            }
        }
    }
    // bitmap_scan과 동일하나, 찾은 영역의 비트를 반전
    else if (sscanf(command, "bitmap_scan_and_flip %s %d %d %5s", name, &start, &count, val_str) == 4) {
        if (strcmp(val_str, "true") == 0) {
            val = true;
        } else if (strcmp(val_str, "false") == 0) {
            val = false;
        }
    
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                printf("%zu\n", bitmap_scan_and_flip(bitmaps[i], start, count, val));  
                fflush(stdout);  
                return;
            }
        }
    }
    // 지정한 인덱스의 비트를 true 또는 false로 설정
    else if(sscanf(command, "bitmap_set %s %d %s", name, &index, val_str) == 3){
        if(strcmp(val_str, "true") == 0){
            val = true;
        } else if (strcmp(val_str, "false") == 0){
            val = false; 
        }

        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                    bitmap_set(bitmaps[i], index, val);
            }        
        }
    }
    // 비트맵 전체 비트를 true 또는 false로 설정
    else if(sscanf(command, "bitmap_set_all %s %s", name, val_str) == 2){
        if(strcmp(val_str, "true") == 0){
            val = true;
        } else if (strcmp(val_str, "false") == 0){
            val = false; 
        }

        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                    bitmap_set_all(bitmaps[i], val);
            }        
        }
    }
    // 지정한 범위의 비트를 한꺼번에에 true 또는 false로 설정
    else if (sscanf(command, "bitmap_set_multiple %s %d %d %5s", name, &start, &count, val_str) == 4) {
        if (strcmp(val_str, "true") == 0) {
            val = true;
        } else if (strcmp(val_str, "false") == 0) {
            val = false;
        }
    
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                bitmap_set_multiple(bitmaps[i], start, count, val);  
                fflush(stdout);  
                return;
            }
        }
    }
    // 비트맵 전체 크기를 출력
    else if(sscanf(command, "bitmap_size %s", name) == 1){
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                    printf("%zu\n", bitmap_size(bitmaps[i]));
                    return;
                }
                
            }
    }
    // 지정한 인덱스의 비트 값이 true인지 확인하여 출력
    else if(sscanf(command, "bitmap_test %s %d", name, &index) == 2){
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                    if(bitmap_test(bitmaps[i],index)){
                        printf("true\n");
                    }else  printf("false\n");

                    return;
                }
                
            }
    }
    // 비트맵의 크기를 지정한 만큼 확장
    else if(sscanf(command, "bitmap_expand %s %d", name, &value) == 2){
        for (i = 0; i < bitmap_cnt; i++) {
            if (strcmp(bitmap_names[i], name) == 0) {
                struct bitmap *new_bitmap = bitmap_expand(bitmaps[i], value);
                if (new_bitmap != NULL) {
                    bitmaps[i] = new_bitmap;  
                }
                return;
            }
        }
    }
    

    /* 종료 명령 */
    else if (strncmp(command, "quit", 4) == 0) {
        exit(0);
    }

    else {
        printf("Invalid command.\n");
    }

}

/* 메인 함수 */
int main() {
    srand(time(NULL));  // 난수 초기화
    char command[100];
    
    while (1) {
        if (!fgets(command, sizeof(command), stdin)) break;
        process_command(command);
    }
    return 0;
}
