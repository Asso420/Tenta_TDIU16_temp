
/*
(a): 
1: Detta händer när två trådar kör samtidigt villkoren i if-satsen: if (s->seats[first_seat + i].reserved == NULL)
    samtidigt. Båda trådarna ckeckar om platsen "first_seat + i" är reserverad eller inte. när de kör samtidigt
    meddelas det att platsen "first_seat + i" är ledig, och varibeln "successfully_reserved++; " plussas på och ena 
    platsen skrivs över den andra platsen. funktionen "screening_book" returnerar inte false trots 

2: Det liknande som problem ovan, två när två trådar försöker flytta samma bookning till olika ställen. 
    t.ex   T1 kör    screening_move(struct screening *s, 1, 2);
           T2 kör    screening_move(struct screening *s, 1, 3);
        först läses av platsen from om den är bokad. "const char *to_move = s->seats[from].reserved;" och 
        både trådar meddelar att plats from "1" är bokad, vilket gör prgrammet går förbi första if-satsen.
        Sen  kollas i andra if-satsen om platsena 2 och 3 är lediga och är det. 
        och via den här "s->seats[to].reserved = to_move;"  flyttas samma boknig till två olika platser dvs till 2 och 3. och funktionen returnerar true. 

3: Detta kan också hända när t.e T1 kör "screening_book" och T2 kör "screening_move" samtidigt
   Det som händer är att T1 kommer att lyckas med bokningen och returnera "successfully_reserved == count;"
    

(b) se koden 

(c): Nej, det ska inte finnas några deadlock.  
        T.ex Cirkle Wait: uppfylls inte för att det finns inga trådar som väntar på dvaranras resurser
        medans de själva håller resurser som de andra trådar väntar på. 

(d): min implementering bara ser till de kritiska områden användas av en tråd åt gången 
    men annars den garanterar att bokningen ska kunna ske parallellt. 

(e): Det är möjligt att tråd 1 mysslyckas och tråd 2 som redan håller på med bokningen lyckas 
    för att min lösning går uu på att släppa en tråd i taget. i detta fall trådet som kommer först
    eller håller på med bokning lyckas. 
    
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
 * Beskrivning av en plats i en salong. Du kan anta att medlemmarna bara
 * modifieras av funktionerna i den här filen.
 */
struct seat {
    // Om bokad: namnet på personen som har bokat platsen. Annars NULL.
    const char *reserved;
};

// Initiera en plats.
void seat_init(struct seat *seat) {
    seat->reserved = NULL;
}


/**
 * En filmvisning (dvs. ett tillfälle då en film visas i en
 * specifik salong). Du kan anta att medlemmarna bara modifieras av
 * funktionerna i den här filen.
 */
struct screening {
    // Array av platser i salongen.
    struct seat *seats;

    // Antal platser i salongen.
    int seat_count;

    // detta lock ska skydda bokningen för varje plats
    struct lock *seat_lock;     // --b

    // Detta lock används när bokningarna ska flyttas
    struct lock  move_lock; // --b

    struct lock reserved_lock; 

};

// Skapa en filmvisning med ett visst antal platser.
struct screening *screening_create(int seat_count) {
    struct screening *s = malloc(sizeof(struct screening));
    s->seats = malloc(sizeof(struct seat) * seat_count);
    s->seat_count = seat_count;
    lock_init(&s->move_lock);
    lock_init(&s->reserved_lock);

    for (int i = 0; i < seat_count; i++) {
        seat_init(&s->seats[i]);
        // locket initieras för varje antal platser som är tillgängliga i salongen
        lock_init(&s->seat_lock[i]);  // --b
    }

    return s;
}

// Destruera en filmvisning. Du kan anta att ingen annan tråd använder
// filmvisnings-objektet när det destrueras.
void screening_destroy(struct screening *s) {
    free(s->seats);
    free(s);
}

// Skriv ut innehållet i en filmvisning. Denna funktion är *inte* en
// del av uppgiften, och du kan därmed anta att den inte körs
// samtidigt som andra funktioner i filen.
void screening_print(struct screening *s) {
    printf("---------\n");
    for (int i = 0; i < s->seat_count; i++) {
        const char *reserved = s->seats[i].reserved;
        if (!reserved)
            reserved = "(free)";
        printf("%2d: %s\n", i, reserved);
    }
}

// Försök att boka platser bredvid varandra för 'count' personer.
// Första platsen är nummer 'first_seat', och resterande platser är
// bredvid den första.
bool screening_book(struct screening *s, int first_seat, int count, const char *names[]) {
    int successfully_reserved = 0;

    // Försök reservera platser:
    for (int i = 0; i < count; i++) {

        // här ska en tråd släppas åt gången för att läsa in data och även ändra varibeln
        // "successfully_reserved++;"
        lock_aquire(&s->seat_lock[i]); // --b
        if (s->seats[first_seat + i].reserved == NULL) {
            s->seats[first_seat + i].reserved = names[i];
            successfully_reserved++;
        } else {
            // locket släpps i fall om programmet behöver breakas
            lock_release(&s->seat_lock[i]);  // --b
            break;
        }
    }

    // Om vi misslyckades med minst en, avboka platserna igen:
    for (int i = 0; i < successfully_reserved; i++) {
        if (successfully_reserved < count) {
            s->seats[first_seat + i].reserved = NULL;
        }
    }
    // locket slpps helt innan prgrammet gå vidare. 
    lock_release(&s->seat_lock[i]); // --b 

    // Meddela om vi lyckades eller inte.

    // lock för att flera trådar ska inte kunna hålla på med  "successfully_reserved"
    // annasrs kan det hända platser dubbelbokas eller vissa platser försvinner. 
    lock_aquire(&s->reserved_lock);
    return successfully_reserved == count;  // --b
    lock_release(&->reserved_lock);
}

// Flytta en bokad plats. Den nya platsen måste vara tom.
bool screening_move(struct screening *s, int from, int to) {

    lock_aquire(&s->move_lock);
    const char *to_move = s->seats[from].reserved;
    if (to_move == NULL)
        lock_release(&s->move_lock);
        return false;
    if (s->seats[to].reserved != NULL)
        lock_release(&s->move_lock);
        return false;

    s->seats[to].reserved = to_move;
    s->seats[from].reserved = NULL;
    lock_release(&s->move_lock);

    return true;
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

int main(void) {
    struct screening *s = screening_create(10);

    // Boka plats 6, 7, och 8.
    const char *names1[] = {
        "Kim", "Alice", "Bob",
    };
    screening_book(s, 6, 3, names1);
    screening_print(s);

    // Boka plats 4, 5, och 6 (går inte, plats 10 är upptagen).
    const char *names2[] = {
        "Kim2", "Alice2", "Bob2",
    };
    screening_book(s, 4, 3, names2);
    screening_print(s);

    // Boka plats 3, 4 och 5 i stället.
    screening_book(s, 3, 3, names2);
    screening_print(s);


    // Flytta på Alice till plats 3 (går inte, den är full).
    screening_move(s, 7, 3);
    screening_print(s);

    // Flytta på Alice till plats 2 i stället.
    screening_move(s, 7, 2);
    screening_print(s);

    screening_destroy(s);

    return 0;
}
