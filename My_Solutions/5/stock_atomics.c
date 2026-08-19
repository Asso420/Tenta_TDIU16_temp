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
        if (compare_and_swap(&s->items[i].price < 0, -1) == -1)
        {
            //free_item = i;
            break;
        }
    }

    if (free_item >= 0) {
        s->items[free_item].name = name;
        s->items[free_item].price = price;
    }

    return free_item;
}

// Beräkna det totala värdet av alla varor i lagret.
int stock_total_price(struct stock *s) {
    int total_price = 0;

    for (int i = 0; i < s->max_items; i++) {
        struct item *item = &s->items[i];
        if (atomic_swap(&item->name != NULL, NULL) == NULL) {
            total_price +=  atomic_read(&item->price);
        }
    }

    return total_price;
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
    for (int i = 0; i < 100; i++) {
        stock_insert(s, "Laptop", 5000);
        stock_insert(s, "GPU", 6000);
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
    int expected_price = 10 * 100 // kaffe
        + 5000 * 100 + 6000 * 100; // laptop + gpu

    int total = stock_total_price(s);
    if (total != expected_price) {
        printf("Total should be %d. Got %d.\n", expected_price, total);
    }

    stock_print(s);

    stock_free(s);

    return 0;
}
