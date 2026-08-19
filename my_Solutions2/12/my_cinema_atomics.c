/*

a:

b:

c:

d:

e:

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
    
};

// Skapa en filmvisning med ett visst antal platser.
struct screening *screening_create(int seat_count) {
    struct screening *s = malloc(sizeof(struct screening));
    s->seats = malloc(sizeof(struct seat) * seat_count);
    s->seat_count = seat_count;

    for (int i = 0; i < seat_count; i++) {
        seat_init(&s->seats[i]);
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
        if (s->seats[first_seat + i].reserved == NULL) {
            atomic_write(s->seats[first_seat + i].reserved = names[i]);
            atomic_add(&s->successfully_reserved,1);

        } else {
            break;
        }
    }

    // Om vi misslyckades med minst en, avboka platserna igen:
    for (int i = 0; i < successfully_reserved; i++) {
        if (successfully_reserved < count) {
            atomic_read(s->seats[first_seat + i].reserved = NULL);
        }
    }

    // Meddela om vi lyckades eller inte.
    return atomic_read(&s->successfully_reserved == count);
}

// Flytta en bokad plats. Den nya platsen måste vara tom.
bool screening_move(struct screening *s, int from, int to) {
    const char *to_move = s->seats[from].reserved;
    if (to_move == NULL)
        return false;
    if (s->seats[to].reserved != NULL)
        return false;

    s->seats[to].reserved = to_move;
    s->seats[from].reserved = NULL;

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

    // Boka plats 4, 5, och 6 (går inte, plats 6 är upptagen).
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
