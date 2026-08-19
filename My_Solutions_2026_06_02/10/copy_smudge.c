/*

a): 
1: Detta kan hända när funktionen "smudge_pxel" körs av två trådar samtidigt vilket i sin 
tur leder till fel beräkning "size" och "total" om dessa värden blir fel, kommer att hela pixelvärdet
att bli fel.  

2: 

3: Detta är för att while loopen (while (data.last_y < p->height);) inuti *picture_smudge väntar på att "last_y"
kommer upp till bildens höjd "p_height" och sen returnera. Men problemet uppstår för att programmet avbrytts om 
" if (data->last_y >= src->height) {
            break;
        }"
i funktionen smudge_worker. Detta leder till de trådar som kör funktionen "smudge_pixel" inte hunnit göra klart sitt
jobb. 


b) i funktionen (*picture_smudge), while loopen väntar på att y_koordinaten som tråden 
börjat arbeta på ska bli lika med bilden höjd. 
    while (data.last_y < p->height)
        ;


c): Genom att implementera två separata semaphorer ( en för start och en för done), kan vi se till 
att trådarana blockeras i början av "smudge_worker" med en sema_down. När funktionen anropas från "*picture_smudge"
sätts sema_up som signalerar att den skapade tråden kan använda resurserna in funktionen "smudge_worker".
När alla trådar har gjord klart sitt jobb ska sema_done sättas till sema_up som signalerar
att "*picture_smudge" kan returnera sin värde utan att bry sig om att missa någon tråd inte är klar. 

se koden

d) se koden

e): Ja, för att semaphorer gör släpper in en tråd i taget och signalerar "klart" när 
alla trådar är klara. 
Och i (d) säkerställer implementationen av lås att de kritiska områden används av en tråd i 
taget och släppa låset när trådet är klar. 

Både lås och semaphorer tillåter att flera trådar kan jobba samtidigt utan att 
störa varandra. 

**Bbs: implementationen går inte att komplera för att det finns syntax fel. har inte hunnit
tänka på implementationen. 


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
 * En datastruktur som beskriver en svartvit bild. Bilden
 * representeras som en 2D-array av pixlar. Varje pixel är ett heltal
 * mellan 0 och 'max_val' (oftast 255). Talet 0 motsvarar svart och
 * 'max_val' motsvarar vit.
 *
 * Notera: Du behöver *inte* synkronisera bild-representationen. Den
 * finns här så att du kan se hur den ser ut.
 */
struct picture {
    // Bildens bredd.
    int width;

    // Bildens höjd.
    int height;

    // Maximalt värde i 'pixels'.
    int max_val;

    // 2D-array av pixlar, pixels[x][y] är pixel på koordinat x, y.
    // Varje pixel har ett värde mellan 0 (vit) och 'max_val' (svart).
    int **pixels;

    struct lock lock_pixels; // för att skydda känslig data för pixelberäkning. // --d


};

// Skapa en ny bild. Bilden är från början fylld med 0:or (= svart).
struct picture *picture_create(int width, int height, int max_val) {
    struct picture *p = malloc(sizeof(struct picture));
    p->width = width;
    p->height = height;
    p->max_val = max_val;
    p->pixels = malloc(sizeof(int *) * width);


    for (int x = 0; x < width; x++) {
        // Notera: calloc är bara malloc, men den initierar till 0.
        // Nedan är ekvivalent med: malloc(sizeof(int) * height)
        p->pixels[x] = calloc(sizeof(int), height);
    }
    return p;
}

// Frigör en bild. Vi antar att ingen använder bilden när den frigörs.
void picture_free(struct picture *p) {
    for (int x = 0; x < p->width; x++) {
        free(p->pixels[x]);
    }
    free(p->pixels);
    free(p);
}

// Inkludera 'picture_load', 'picture_save' och 'picture_display'. Du
// kan titta på dem om du vill, men de är inte en del av uppgiften.
#include "picture_utils.h"


/**
 * Koden som är relevant i uppgiften finns här nedanför.
 */

// Hjälpfunktion som beräknar ett utsmetat pixelvärde för en viss
// pixel. Detta görs genom att beräkna medelvärdet av pixlarna (x, y)
// till (x + size - 1, y). Du kan anta att funktionen endast används
// av 'smudge_worker'.
static int smudge_pixel(struct picture *p, int x, int y, int size) {
    int total = 0;
    lock_acquire(&p->lock_pixels); // --d
    if (x + size > p->width) {
        size = p->width - x;
        lock_release(&p->lock_pixels);  // --d
    }
    lock_release(&p->lock_pixels);     // --d

    for (int curr = x; curr < x + size; curr++) {
        lock_acquire(&p->lock_pixels);  // --d
        total += p->pixels[curr][y];
        lock_release(&p->lock_pixels);  // --d
    }

    if (size == 0)
        size = 1;
    return total / size;
}


/**
 * Data som delas mellan 'picture_smudge' och 'smudge_worker'.
 */
struct smudge_data {
    // Originalbild.
    struct picture *src;

    // Bild som ska produceras.
    struct picture *dest;

    // Hur mycket ska originalbilden smetas ut?
    int smudge_size;

    // Koordinat för den senaste pixeln som någon tråd har börjat
    // bearbeta.
    int last_x;
    int last_y;

    struct semaphore sema_start; // för att signalera trådarana att de kan starta jobba  --C
    struct semaphore sema_done;    // ör att signalera att alla trådar är klara och inte har mer jobba att göra.  --C
};

// Funktion som körs av de trådar som startas av 'picture_smudge'.
// Dessa trådar hjälper till att modifiera den givna bilden, pixel för
// pixel. Du kan anta att funktionen endast anropas från
// 'picture_smudge'.
static void smudge_worker(struct smudge_data *data) {
    struct picture *src = data->src;
    struct picture *dest = data->dest;

    sema_down(&data->sema_start);  // -- C
    while (true) {

        data->last_x++;
        if (data->last_x >= src->width) {
            data->last_y++;
            data->last_x = 0;
        }

        if (data->last_y >= src->height) {
            break;
        }

        int x = data->last_x;
        int y = data->last_y;

        dest->pixels[x][y] = smudge_pixel(src, x, y, data->smudge_size);

      sema_up(&data->sema_done);    // --C
    }
}

// Använd 'threads' trådar för att smeta ut innehållet i bilden 'p'.
// Hur mycket innehållet smetas ut styrs av 'smudge_size'. Funktionen
// antar att ingen annan tråd modifierar 'p' när 'picture_smudge'
// anropas (men det ska gå att köra 'picture_smudge' flera gånger,
// till och med från olika trådar). Funktionen modifierar inte 'p',
// utan returnerar en ny bild som innehåller det utsmetade innehållet.
struct picture *picture_smudge(struct picture *p, int smudge_size, int threads) {
    struct smudge_data data;
    data.src = p;
    data.dest = picture_create(p->width, p->height, p->max_val);
    data.last_x = -1;
    data.last_y = 0;
    data.smudge_size = smudge_size;
    sema_init(&data->sema_start, 0);    // --C
    sema_init(&data->sema_done, 0);     // --C

    for (int i = 0; i < threads; i++) {
        sema_up(&p->sema_start);        // --C
        thread_new(&smudge_worker, &data);
    }

    // while (data.last_y < p->height)
    //     ;
    for (i = data.last_y; i < p->height; i++) {     // --C
    sema_down(&data->sema_done, 0);     // --C
}

    return data.dest;
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

int main(int argc, const char *argv[]) {
    const char *input_file = "pintos.ppm";
    const char *output_file = "output.ppm";
    if (argc >= 2)
        input_file = argv[1];
    if (argc >= 3)
        output_file = argv[2];

    struct picture *p = picture_load(input_file);

    printf("Original:\n");
    picture_display(p);

    struct picture *smudged = picture_smudge(p, 4, 2);
    picture_free(p);

    printf("Smudged:\n");
    picture_display(smudged);
    picture_save(smudged, output_file);
    picture_free(smudged);
    return 0;
}
