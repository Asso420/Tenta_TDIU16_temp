/*

a)
1: Detta kan hända när två trådar kör funktionen "bank_new_account". Exampelvis om det ska 
skapas två konto med namn A och B. Då går programmet förbi if satsen inuti for loopen då 
har har det skapats två kontonummer till A och B med index "i". 
När programmet kommer till nedan och två trådar kör den samtidigt, sparas uppgifter för ena 
kunden (A eller B) i arrayen accounts. Och detta går ihåop med att det totala saldon visar olika 
varje gång, för att när två trådar kör nedan, då sparas ena saldon för till example A eller B i vårt 
fall. 
    "b->accounts[unused].name = strdup(name);
     b->accounts[unused].balance = balance;"
Det fins också en förklaring till att varför den totatal saldon olika vid varje körning, då om två trådar kör
raden nedan då finns det risken att ena kontonumret missas att skapas dvs olika antal konto skapas vid varje körning:         
            "if (b->accounts[i].name == NULL) {
             unused = i;" 


2: Detta händer när två trådar samtidigt kör raden "to->balance += close->balance;" i bank_close_account.
Exampelvis: om det ska överföras från konto A med "close_id = 1" och "to_id = 2" till konto B med "close_id = 3"
och "to_id = 4". Så när två trådar samtidigt ska uppdatera "to->balance" kan det hända att saldo från konto A överförs till
konto B. 

3: Detta beror på att "bank_close_account" har inte hunnits att stängas av ordentligt.


b) se koden
 

c) : Nej, det finns inte risk till ett deadlock för lösningen i (b), då kravet för "cirkular wait"
inte uppfylls, dvs det finns inga trådar som väntar på varndras resurser i en cyrkel. Däremot
No preemption uppfylls: användning av lås, trådar släpper resursen när de är klara.
Mutual Exclusion: uppfylls då lås granterar att resurser används av en tråd i taget. 
 

d)
Ja, Eftersom lås har använts för att skydda bara de kritiska områden. 


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
    struct lock new_account; // --b
    struct lock close_account;  // --b
};

// Skapa en bank och initiera den.
struct bank *bank_create(void) {
    struct bank *b = malloc(sizeof(struct bank));
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        struct account *a = &b->accounts[i];
        a->name = NULL;
        a->balance = 0;
        lock_init(&b->new_account);  // --b
        lock_init(&b->close_account);  // --b
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

        lock_acquire(&b->new_account);  // --b
        if (b->accounts[i].name == NULL) {   
            unused = i;
            lock_release(&b->new_account);  // --b
            break;
            
        }
        lock_release(&b->new_account);  // --b
    }

    if (unused < 0)
        return -1;
    lock_acquire(&b->new_account); // --b
    b->accounts[unused].name = strdup(name);
    b->accounts[unused].balance = balance;
    lock_release(&b->new_account);  // --b
    return unused;
}

// Stäng ett konto (= ta bort ett konto) i banken 'b'. För att inte
// banken ska förlora pengar så ska eventuellt saldo på kontot som
// stängs ('close_id') överföras till ett annat konto som anges av
// 'to_id'. Om allt går bra returneras 'true'. Om antingen 'close_id'
// eller 'to_id' refererar till ett konto som inte är aktivt ska inget
// hända och funktionen ska returnera 'false'. Detta ska kunna göras
// från flera trådar samtidigt.
bool bank_close_account(struct bank *b, int close_id, int to_id) {
    struct account *close = &b->accounts[close_id];
    struct account *to = &b->accounts[to_id];

    if (close->name == NULL)
        return false;
    if (to->name == NULL)
        return false;

    lock_acquire(&b->close_account);  // --b
    to->balance += close->balance;
    lock_release(&b->close_account);  // --b

    free(close->name);
    close->name = NULL;
    close->balance = 0;

    return true;
}

// Skriv ut en sammanfattning av alla konton i banken. Detta ska kunna
// göras samtidigt som andra trådar manipulerar kontona i banken med
// 'bank_new_account' och 'bank_close_account'. Du kan dock anta att
// bara en tråd anropar 'bank_print'.
void bank_print(struct bank *b) {
    int total_balance = 0;
    printf("%3s | %-10s | %5s\n", "id", "namn", "saldo");
    printf("---------------------------\n");

    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        struct account *a = &b->accounts[i];
        if (a->name != NULL) {
            total_balance += a->balance;
            printf("%3d | %-10s | %5d\n", i, a->name, a->balance);
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
