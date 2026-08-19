/*

a) 1) Antag att T1 ("A", 10) och T2 ("B", 20) kör funktionen stock_insert samtidigt då nedan händer:

    T1 kör till raden if (s->items[i].name == NULL); och ser att det finns ett namn och sedan sätter 
    free_item till index '0' och kör vidare. Direkt efter kommer T2 till samma rad och ser att det finns ett 
    item namn och stätter free_item till '0' och kör vidare till raderna       s->items[free_item].name = name;
                                                                                s->items[free_item].price = price;

    Eftersom nu har två items med två olika priser och namn som har en och samma index dvs. '0' skrivs över ena 
    item och den andra försvinner. t.ex om T2 hinner skriva namn och pris -> s->items[0] == (B, 20)
    och det blir överskrivet när T1 kommer till samma rader då har vi s->items[0] == (A, 10) som kommer att returneras. 

    2) 
    3) detta har med 
b)

c) Nej det finns ingen risk till att implementationen går i deadlock läget för att det ska gå i deadlock 
läget behöves en cikle_wait krav uppfyllas vilket inte gör det i implementationen. I implementationen har används 
olika lock för att slippa blanda ihop dem och även de släps ordendtligt när de är klara med sina reurser. 


d) Ja, för att, impleemntationen initiarar lika många lock instanser som storleken på arrayen, det vill säga 
    alla item i arrayen kommer att ha tillgång till sin egen lock. detta gör att de inte allockerar varandras locks
    och kan köra paralellt. OBS-> När jag bytte till pekare (från ett vanligt lock) fick jag "// --b initiara lock för items"
    som jag inte att är relevant för synkroniserings prblemen. 


 */

#include "wrap/thread.h"
#include "wrap/synch.h"
#include "wrap/atomics.h"
#include "wrap/timer.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/**
 * Representation av en vara som lagras på ett lager.
 */
struct item {
    // Namn på varan.
    const char *name;

    // Pris på varan.
    int price;
};

/**
 * Representerar ett lager som innehåller upp till 'max_items' varor.
 */
struct stock {
    // Array av varor i lagret.
    struct item *items;

    // Storlek av arrayen 'items'.
    int max_items;

    struct lock item_lock;  // --b

    struct lock total_lock; // --b

    struct lock price_lock;  // --b

};

// Skapa ett lager med plats för upp till 'max_items' varor.
struct stock *stock_create(int max_items) {
    struct stock *s = malloc(sizeof(struct stock));

    s->max_items = max_items;
    s->items = malloc(sizeof(struct item) * max_items);
    for (int i = 0; i < max_items; i++) {
        struct item *item = &s->items[i];
        item->name = NULL;
        item->price = -1;
        lock_init(&s->item_lock);  // --b initiara lock för items
        lock_init(&s->total_lock);   // --b initiara lock för items
        lock_init(&s->price_lock);   // --b initiara lock för items
    }
    return s;
}

// Förstör ett lager. Antar att ingen annan tråd använder lagret.
void stock_free(struct stock *s) {
    free(s->items);
    free(s);
}

// Skriv ut innehållet i ett lager. Detta antas inte ske samtidigt som
// någon annan tråd arbetar med lagret 's', eller samtidigt som någon
// annan tråd skriver ut varor till terminalen.
void stock_print(struct stock *s) {
    for (int i = 0; i < s->max_items; i++) {
        struct item *item = &s->items[i];
        if (item->name != NULL) {
            printf("%3d: %-15s %4d\n", i, item->name, item->price);
        }
    }
}

// Lägg till en ny vara i lagret. Om lagret är fullt ska -1 returneras,
// annars ska varans ID returneras.
int stock_insert(struct stock *s, const char *name, int price) {
    int free_item = -1;

    
    for (int i = 0; i < s->max_items; i++) {
        lock_acquire(&s->item_lock);    // --b
        if (s->items[i].name == NULL) {
            free_item = i;
            lock_release(&s->item_lock);    // --b
            break;
        }
        lock_release(&s->item_lock);    // --b
    }
    lock_acquire(&s->item_lock);  // --b
    if (free_item >= 0) {
        s->items[free_item].name = name;
        s->items[free_item].price = price;   
    }
    lock_release(&s->item_lock);    // --b
    

    return free_item;
}

// Beräkna det totala värdet av alla varor i lagret.
int stock_total_price(struct stock *s) {
    int total_price = 0;
    for (int i = 0; i < s->max_items; i++) {
        struct item *item = &s->items[i];  
        if (item->name != NULL) {
            lock_acquire(&s->total_lock);   // --b
            total_price += item->price;
            lock_release(&s->total_lock);   // --b
        }
    
    }
    return total_price;
}

// Uppdatera priset på de varor som anges i arrayen 'item_ids' till
// 'new_price'. Du kan anta att de index som finns i 'item_ids' är
// sorterade i stigande ordning. Denna operation ska ske som en enhet,
// det ska alltså inte gå att observera att enbart en del av varorna
// har ändrat pris. För att uppdatera priset på vara nummer 1 och 5
// till 200 kr kan funktionen anropas som följer:
//
// int ids[2] = { 1, 5 };
// stock_update_price(&stock, ids, 2, 200);
void stock_update_price(struct stock *s, int *item_ids, int id_count, int new_price) {
    for (int i = 0; i < id_count; i++) {
        lock_acquire(&s->price_lock);
        int item_id = item_ids[i];
        struct item *item = &s->items[item_id];
        item->price = new_price;
        lock_release(&s->price_lock);
    }
}

/**
 * Huvudprogram.
 *
 * Programmet finns för att visa hur koden ovan kan köras. Den är inte
 * nödvändigtvis designad för att visa alla möjliga problem i
 * implementationen.
 *
 * Ändringar nedanför denna kommentar kommer att ignoreras vid
 * bedömning av din lösning. Uppgiften ska alltså lösas utan att lägga
 * till synkronisering här nedanför. Det går självklart bra att
 * modifiera koden nedanför för att testa din lösning.
 */

void thread_fn(struct stock *s, struct semaphore *done) {
    int gpus[100];

    for (int i = 0; i < 100; i++) {
        stock_insert(s, "Laptop", 5000);
        gpus[i] = stock_insert(s, "GPU", 6000);
    }

    sema_up(done);

    // Uppdatera priserna på GPU:er.
    for (int i = 0; i < 1000; i++) {
        stock_update_price(s, gpus, 100, 6000);
        stock_update_price(s, gpus, 100, 7000);
    }

    sema_up(done);
}

int main(void) {
    struct semaphore done;
    struct stock *s = stock_create(400);

    sema_init(&done, 0);
    thread_new(&thread_fn, s, &done);

    for (int i = 0; i < 100; i++) {
        stock_insert(s, "Kaffe", 10);
    }

    sema_down(&done);

    // Priset för allt som sätts in borde vara:
    int initial_price = 10 * 100 // kaffe
        + 5000 * 100 + 6000 * 100; // laptop + gpu
    int raised_price = initial_price + 1000 * 100;

    for (int i = 0; i < 100; i++) {
        int total = stock_total_price(s);
        if (total != initial_price && total != raised_price)
            printf("Total should be either %d or %d. Got %d.\n", initial_price, raised_price, total);
    }

    sema_down(&done);

    stock_print(s);

    stock_free(s);

    return 0;
}
