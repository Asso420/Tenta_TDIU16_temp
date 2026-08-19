/*

a) se koden


b)


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

/* Funktionen 'strdup' i standardbiblioteket är ekvivalent med detta:

char *strdup(const char *str) {
    int len = strlen(str) + 1;
    char *result = malloc(len);
    strncpy(result, str, len);
    return result;
}
*/

// Maximalt antal konton i banken.
#define MAX_ACCOUNTS 200

/**
 * Ett enskilt konto i banken.
 */
struct account {
    // Namnet på ägaren av kontot. Om kontot är oanvänt så är namnet NULL.
    char *name;

    // Kontots saldo, i kronor.
    int balance;
};

/**
 * En bank. Innehåller en samling konton.
 */
struct bank {
    struct account accounts[MAX_ACCOUNTS];
};

// Skapa en bank och initiera den.
struct bank *bank_create(void) {
    struct bank *b = malloc(sizeof(struct bank));
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        struct account *a = &b->accounts[i];
        a->name = NULL;
        a->balance = 0;
    }
    return b;
}

// Frigör en bank som tidigare blivit skapad med bank_create. Du kan
// anta att ingen annan tråd använder 'b' när 'bank_free' anropas.
void bank_free(struct bank *b) {
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        struct account *a = &b->accounts[i];
        if (a->name != NULL) {
            free(a->name);
        }
    }
    free(b);
}

// Skapa ett nytt konto i banken. Hittar första kontot som inte
// används i 'b' och sparar 'name' och 'balance' på den platsen.
// Returnerar sedan kontonumret som skapades. Detta ska kunna göras
// från flera trådar samtidigt. Om det inte finns plats för fler
// konton ska -1 returneras.
int bank_new_account(struct bank *b, const char *name, int balance) {
    int unused = -1;
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        if (atomic_read(&b->accounts[i].name), NULL) {  // --a
            unused = i;
            break;
        }
    }

    if (unused < 0)
        return -1;

    atomic_write(&b->accounts[unused].name, strdup(name)); // --a
    atomic_write(&b->accounts[unused].balance, balance);    // --a
    return unused;
}

// Stäng ett konto (= ta bort ett konto) i banken 'b'. För att inte
// banken ska förlora pengar så ska eventuellt saldo på kontot som
// stängs ('close_id') överföras till ett annat konto som anges av
// 'to_id'. Om allt går bra returneras 'true'. Om antingen 'close_id'
// eller 'to_id' refererar till ett konto som inte är aktivt ska inget
// hända och funktionen ska returnera 'false'. Detta ska kunna göras
// från flera trådar samtidigt.
//
// Notera: Inte en del av denna uppgift.
bool bank_close_account(struct bank *b, int close_id, int to_id) {
    struct account *close = &b->accounts[close_id];
    struct account *to = &b->accounts[to_id];

    if (close->name == NULL)
        return false;
    if (to->name == NULL)
        return false;

    to->balance += close->balance;

    free(close->name);
    close->name = NULL;
    close->balance = 0;

    return true;
}

// Skriv ut en sammanfattning av alla konton i banken. Detta ska kunna
// göras samtidigt som andra trådar manipulerar kontona i banken med
// 'bank_new_account' och 'bank_close_account'. Du kan dock anta att
// bara en tråd anropar 'bank_print'.
//
// Notera: Du behöver inte modifiera den här funktionen i denna uppgift.
void bank_print(struct bank *b) {
    int total_balance = 0;
    printf("%3s | %-10s | %5s\n", "id", "namn", "saldo");
    printf("---------------------------\n");

    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        struct account *a = &b->accounts[i];
        // Raden nedanför är ny, 'a->name' ersatt med 'name' i resten av funktionen
        const char *name = atomic_read(&a->name);
        if (name != NULL) {
            // Raden nedanför är ny, 'a->balance' ersatt med 'balance' i resten av funktionen
            int balance = atomic_read(&a->balance);
            total_balance += balance;
            printf("%3d | %-10s | %5d\n", i, name, balance);
        }
    }

    printf("Totalt saldo: %d\n", total_balance);
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

// Sätt variabeln till:
// -> 1 för att bara testa bank_new_account
// -> 2 för att också testa bank_close_account
// -> 3 för att testa bank_close_account samtidigt som bank_print
const int test_mode = 1;

void thread_fn(struct bank *b, struct semaphore *done) {
    int last_created = 0;

    for (int i = 0; i < MAX_ACCOUNTS / 2; i++) {
        char name[32];
        snprintf(name, 32, "B%d", i);
        last_created = bank_new_account(b, name, 100);
    }

    sema_up(done);

    if (test_mode > 1) {
        for (int i = 0; i < last_created; i++) {
            bank_close_account(b, i, i + 1);
        }

        sema_up(done);
    }
}

int main(void) {
    struct semaphore done;
    struct bank *b = bank_create();
    sema_init(&done, 0);

    thread_new(&thread_fn, b, &done);

    for (int i = 0; i < MAX_ACCOUNTS / 2; i++) {
        char name[32];
        snprintf(name, 32, "A%d", i);
        bank_new_account(b, name, 100);
    }

    sema_down(&done);

    if (test_mode == 2) {
        for (int i = MAX_ACCOUNTS - 1; i > 0; i--) {
            bank_close_account(b, i, i - 1);
        }

        sema_down(&done);
    } else if (test_mode == 3) {

        for (int i = 0; i < 3; i++)
            bank_print(b);

        sema_down(&done);
    }

    bank_print(b);

    bank_free(b);
    return 0;
}
