/*

a)
    1) detta är för att om två trådar samtidigt kör painter_worker och ena 
    tråden passerar if_satsen i raden if (poly.points == NULL || poly.count <= 0) break; och den 
    andra inte upfyller kraven och hoppar ut ur whlie loopen och frigör innehållet i tmp (free(tmp)) vilket innehåller våran 
    polygon, då kraschar programmet.


    2: Om Två trådar kör funktionen painter_worker samtidigt och kommer åt raden p->polygon_count--; då iställen för 
    att decrementeras två gånger decrementeras en gå och däför ritas polygonen mer än gång.

b)         while (p->polygon_count <= 0) väntar tills att arrayen har blivit tomt
            ;

            while (p->polygon_count >= PAINTER_QUEUE_SIZE) -- väntar på att den innre arrayen blir full
            ;

c)

d)

e)


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
#include "polygon_utils.h"

/**
 * struct point finns definierad i 'polygon_utils.h' som följer:
 *
 * struct point {
 *     int x;
 *     int y;
 * };
 *
 * Den behöver inte modifieras för att lösa uppgiften.
 */


/**
 * En datastruktur som beskriver ett 2-dimensionellt rutnät. Varje
 * cell innehåller ett heltal som räknar hur många polygoner som
 * täcker motsvarande pixel på skärmen.
 *
 * Du kommer inte att behöva modifiera 'grid' med tillhörande
 * funktioner (grid_create, grid_free, och grid_print) för att lösa
 * uppgiften.
 */
struct grid {
    // 2D-array av heltal. Indexeras som cells[x][y].
    int **cells;

    // Anger bredden av 'cells'.
    int width;

    // Anger höjden av 'cells'.
    int height;
};

// Skapa och initiera ett rutnät.
struct grid *grid_create(int width, int height) {
    struct grid *g = malloc(sizeof(struct grid));
    g->width = width;
    g->height = height;
    g->cells = malloc(sizeof(int *) * width);

    for (int x = 0; x < width; x++) {
        g->cells[x] = malloc(sizeof(int) * height);
        for (int y = 0; y < height; y++) {
            g->cells[x][y] = 0;
        }
    }

    return g;
}

// Avallokera ett rutnät.
void grid_free(struct grid *grid) {
    for (int x = 0; x < grid->height; x++) {
        free(grid->cells[x]);
    }

    free(grid->cells);
    free(grid);
}

// Skriv ut ett rutnät.
void grid_print(struct grid *grid) {
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            if (grid->cells[x][y] == 0) {
                printf(" ");
            } else {
                printf("%d", grid->cells[x][y]);
            }
        }
        printf("\n");
    }
}

// Storleken på den interna kön i 'painter'.
#define PAINTER_QUEUE_SIZE 4

/**
 * Datastruktur för att lagra polygoner lite enklare.
 */
struct polygon {
    struct point *points;
    int count;
};

/**
 * Datastruktur som används för att rita polygoner på en 2D-grid med
 * flera trådar parallellt. Datastrukturen innehåller dels nuvarande
 * resultat, men också en kö med polygoner som ska ritas ut.
 */
struct painter {
    // Resultatbilden.
    struct grid *output;

    // Antal trådar som arbetar.
    int thread_count;

    // Lista över polygoner som ska ritas ut. Används som en cirkulär kö.
    struct polygon polygons[PAINTER_QUEUE_SIZE];

    // Antal element som har lagts till i 'polygon'.
    int polygon_count;

    // Första tomma platsen i 'polygons'.
    int polygon_head;

    // Nästa element som ska tas bort ur 'polygons'.
    int polygon_tail;

    struct semaphore sema_start;        //-- c

    struct semaphore sema_done;     //-- c

    struct lock count_lock;     //-- c och //-- d
};

// Funktion som startas av 'painter_create'. Vi antar att den inte
// anropas någon annan stans ifrån.
static void painter_worker(struct painter *p) {
    struct grid *temp = grid_create(p->output->width, p->output->height);

    while (true) {
        // Hämta en polygon från kön i 'p':
        sema_down(&p->sema_start);      //-- c

        // lock_acquire(&p->count_lock);
        // p->polygon_count <= 0;
        // lock_release(&p->count_lock);

        struct polygon poly = p->polygons[p->polygon_tail];
        p->polygon_tail = (p->polygon_tail + 1) % PAINTER_QUEUE_SIZE;
        lock_acquire(&p->count_lock); // --d
        p->polygon_count--;
        lock_release(&p->count_lock);

        // Om polygonen är tom så bad 'painter_exit' oss att avsluta.
        if (poly.points == NULL || poly.count <= 0)
            break;

        // Rita polygonen till 'temp':
        for (int x = 0; x < temp->width; x++) {
            for (int y = 0; y < temp->height; y++) {
                struct point p = { x, y };
                // Notera: point_in_poly är dyr.
                if (point_in_poly(p, poly.points, poly.count)) {
                    temp->cells[x][y] = 1;
                } else {
                    temp->cells[x][y] = 0;
                }
            }
        }

        // Nu har vi vår polygon i 'temp'. Nu kan vi kopiera in den i
        // 'p->output':
        for (int x = 0; x < temp->width; x++) {
            for (int y = 0; y < temp->height; y++) {
                p->output->cells[x][y] += temp->cells[x][y];
            }
        }
    }
    sema_up(&p->sema_done);         //-- c

    free(temp);
}

// Skapa en 'painter'. Anger storlek på bilden som ska skapas, samt
// antal trådar som ska användas för att rita polygoner.
struct painter *painter_create(int width, int height, int thread_count) {
    struct painter *p = malloc(sizeof(struct painter));
    p->output = grid_create(width, height);
    p->thread_count = thread_count;
    p->polygon_count = 0;
    p->polygon_head = 0;
    p->polygon_tail = 0;
    sema_init(&p->sema_start, 0);           //-- c
    sema_init(&p->sema_done, 0);        //-- c
    lock_init(&p->count_lock);

    // Starta trådar.
    for (int i = 0; i < thread_count; i++) {
        sema_up(&p->sema_start);        //-- c
        thread_new(&painter_worker, p);
    }

    for (int i = 0; i < thread_count; i++) {
        sema_down(&p->sema_done);  //-- c

    }

    return p;
}

// Lägg till en polygon som ska ritas ut. Du kan anta att maximalt en
// tråd kör 'painter_draw' samtidigt. Om den interna kön i 'p' är full
// ska funktionen vänta tills det finns plats. Vi antar att 'points'
// inte är NULL och att 'count' är större än 0, förutom när
// 'painter_draw' anropar funktionen.
void painter_draw(struct painter *p, struct point *points, int count) {
    // while (p->polygon_count >= PAINTER_QUEUE_SIZE)
    //     ;
    for (int i = 0; i >= PAINTER_QUEUE_SIZE; i++) {
        sema_up(&p->sema_start);   //-- c
    }

    p->polygons[p->polygon_head].points = points;
    p->polygons[p->polygon_head].count = count;
    p->polygon_head = (p->polygon_head + 1) % PAINTER_QUEUE_SIZE;
    lock_acquire(&p->count_lock);   //-- c
    p->polygon_count++;
    lock_release(&p->count_lock);   //-- c
}

// Anropas för hämta resultatet från 'p'. Väntar på att alla trådar
// blir färdiga, och returnerar den slutgiltiga bilden.
struct grid *painter_finish(struct painter *p) {
    // Be trådarna att avsluta sig genom att skicka en speciell polygon.
    for (int i = 0; i < p->thread_count; i++)
        painter_draw(p, NULL, -1);

    // Spara bilden, avallokera 'p' och returnera resultatet.
    struct grid *result = p->output;
    free(p);
    return result;
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

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(*(x)))

int main(void) {
    struct painter *p = painter_create(32, 25, 4);

    struct point poly0[] = {
        { 1, 3 }, { 3, 1 }, { 6, 1 }, { 6, 10 }, { 3, 10 }, { 3, 3 },
    };
    painter_draw(p, poly0, ARRAY_COUNT(poly0));

    struct point poly1[] = {
        { 8, 10 }, { 10, 8 }, { 14, 8 }, { 16, 10 }, { 16, 14 }, { 10, 17 }, { 16, 17 },
        { 16, 19 }, { 8, 19 }, { 8, 16 }, { 14, 13 }, { 14, 10 },
    };
    painter_draw(p, poly1, ARRAY_COUNT(poly1));
    painter_draw(p, poly1, ARRAY_COUNT(poly1));

    struct point poly2[] = {
        { 16, 3 }, { 18, 1 }, { 22, 1 }, { 24, 3 }, {23, 5}, { 24, 8 }, { 22, 10 },
        { 18, 10 }, { 16, 8 }, { 21, 8 }, { 21, 6 }, { 18, 6 }, { 18, 4 },
        { 21, 4 }, { 21, 3 },
    };
    painter_draw(p, poly2, ARRAY_COUNT(poly2));
    painter_draw(p, poly2, ARRAY_COUNT(poly2));
    painter_draw(p, poly2, ARRAY_COUNT(poly2));

    struct point poly3[] = {
        { 20, 15 }, { 25, 10 }, { 30, 15 }
    };
    painter_draw(p, poly3, ARRAY_COUNT(poly3));

    struct point poly4[] = {
        { 20, 15 }, { 25, 10 }, { 30, 15 }, { 30, 22 }, { 20, 22 }
    };
    painter_draw(p, poly4, ARRAY_COUNT(poly4));

    struct grid *g = painter_finish(p);
    grid_print(g);
    grid_free(g);

    return 0;
}
