#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "faction.h"
#include "galaxy.h"
#include "conquest.h"
#include "market.h"
#include "mission.h"
#include "outfit.h"
#include "save.h"
#include "ship.h"
#include "story.h"
#include "veyr.h"
#include "world.h"

/* Step flags. */
#define F_VEYR   1      /* only once the Veyr have come */
#define F_OATH   2      /* the ultimatum: no contract, the oath itself */
#define F_REVEAL 4      /* finishing it reveals the hive */
#define F_RIG    8      /* accepting it gives you a level 1 containment rig */

typedef struct {
    const char *title;
    const char *brief[5];
    const char *done;
    u8  type;              /* MI_* (0 for the oath) */
    const char *dest;      /* system name; 0 = none */
    u8  nav;               /* NAV_STATION / NAV_PLANET */
    u8  commodity, units;
    u8  target_def, wanted;  /* bounties: the hull and the story name (missions_wanted_story) */
    u16 pay;
    s8  rep;               /* standing with this side */
    u8  wait;              /* game hours before the next orders */
    u8  flags;
} Step;

#define S NAV_STATION
#define P NAV_PLANET

/* ------------------------------------------------------------------------ */
/* The UFS: Commander Iris Hale, Naval Intelligence                         */
/* ------------------------------------------------------------------------ */
static const Step ufs_steps[] = {
    { "SEALED ORDERS",
      { "\"YOU'RE THE CIVILIAN WITH THE BEAT-UP SHIP. GOOD.",
        "NOBODY WATCHES A NOBODY. CARRY THESE ORDERS TO GANTRY,",
        "UNOPENED. IF THE SEAL'S BROKEN, SO ARE YOU.\"", 0, 0 },
      "\"SEAL INTACT. MAYBE YOU'RE USEFUL AFTER ALL.\"",
      MI_COURIER, "GANTRY", S, 0, 0, 0, 0, 1500, 8, 14, 0 },
    { "FEVER ON ISOLDE",
      { "\"A FEVER IS BURNING THROUGH THE DOMES ON ISOLDE.",
        "THE NAVY HAS BETTER THINGS TO DO THAN HAUL MEDICINE.",
        "YOU DON'T. TEN CRATES. FAST, IF YOU HAVE A HEART.\"", 0, 0 },
      "\"THE DOMES ARE QUIET AGAIN. QUIET IS GOOD.\"",
      MI_SUPPLY, "ISOLDE", P, COMM_MEDICAL, 10, 0, 0, 2500, 8, 20, 0 },
    { "THE RAVEN PROBLEM",
      { "\"RAVEN IS UFS ON PAPER. IN TRUTH IT'S CUT OFF,",
        "AND THE PIRATES KNOW IT. THEY'RE BLEEDING IT DRY.",
        "KILL FOUR OF THEM. LET THE REST SPREAD THE WORD.\"", 0, 0 },
      "\"FOUR DOWN. RAVEN WILL SLEEP TONIGHT.\"",
      MI_INTERCEPT, "RAVEN", S, 0, 4, 0, 0, 3500, 10, 20, 0 },
    { "THE DEFECTOR",
      { "\"AN FSA OFFICER WANTS OUT. SHE SAYS SHE HAS NAMES.",
        "SHE'S WAITING FOR YOU. FLY HER TO AURORA, TO ME.",
        "IF THE FSA FINDS OUT, THEY WON'T FORGIVE YOU.\"", 0, 0 },
      "\"HER NAMES CHECK OUT. THE FSA IS ROTTEN TO THE CORE.\"",
      MI_PASSENGER, "AURORA", S, 0, 1, 0, 0, 3000, 8, 20, 0 },
    { "LISTENING POST NINE",
      { "\"POST NINE STOPPED TALKING TWO WEEKS AGO.",
        "IT SITS IN THE DARK OF BELLWETHER. FIND IT,",
        "AND TELL ME WHAT IT HEARD BEFORE IT DIED.\"", 0, 0 },
      "\"ITS LAST RECORDING IS... SCRATCHING. ON THE HULL.\"",
      MI_SURVEY, "BELLWETHER", S, 0, 0, 0, 0, 3500, 8, 24, 0 },
    { "WANTED: THE FOX",
      { "\"AN FSA RAIDER CALLED THE FOX HITS OUR CONVOYS",
        "AND RUNS FOR SCORIA. HE FLIES A RENEGADE.",
        "BRING ME HIS TRANSPONDER. HE WON'T NEED IT.\"", 0, 0 },
      "\"THE FOX IS DEAD. THE FSA WILL HAVE NOTICED.\"",
      MI_BOUNTY, "SCORIA", S, 0, 0, DEF_RENEGADE, 0, 5000, 10, 24, 0 },
    { "GUNS FOR THE ENCLAVE",
      { "\"OUR PEOPLE IN WICKHAM ARE SURROUNDED BY FSA SPACE.",
        "TWELVE CRATES OF WEAPONS. THE ONLY WAY IN IS THROUGH",
        "THEIRS. DON'T STOP FOR ANYONE.\"", 0, 0 },
      "\"WICKHAM IS ARMED. LET THEM COME.\"",
      MI_CARGO, "WICKHAM", S, 9, 12, 0, 0, 6000, 10, 24, 0 },
    { "SYNDICATE RAIDERS",
      { "\"A SYNDICATE CREW HAS TAKEN TO HUNTING IN CALLOW.",
        "SIX SHIPS. THE NAVY WON'T SEND A SQUADRON FOR SIX.",
        "I'M SENDING YOU.\"", 0, 0 },
      "\"THE RIM WILL HATE YOU FOR THAT. GOOD.\"",
      MI_INTERCEPT, "CALLOW", S, 0, 6, 0, 0, 6500, 10, 24, 0 },
    { "THE BLACK SITE",
      { "\"A SURVEY TEAM WENT DARK IN THE GAP. LAST SIGNAL:",
        "'IT'S NOT ONE OF OURS. IT'S NOT ONE OF ANYONE'S.'",
        "THERE IS NO PORT AND NO FUEL OUT THERE. GO LOOK.\"", 0, 0 },
      "\"A VEYR HUSK. OLD. BUT THE BURN MARKS ARE NEW.\"",
      MI_SURVEY, "THE GAP", S, 0, 0, 0, 0, 7000, 8, 30, 0 },
    { "THE OATH",
      { "\"HIGH COMMAND HAS READ YOUR FILE. THEY WANT YOU",
        "IN UNIFORM: UFS RANK, UFS HULLS, UFS ORDERS.",
        "SOMETHING IS COMING FROM THE RIM, PILOT. WE KNOW IT.",
        "WHEN IT COMES, I WANT YOU ON OUR SIDE OF THE LINE.\"", 0 },
      "YOU SWORE THE OATH. WELCOME TO THE UFS.",
      0, 0, S, 0, 0, 0, 0, 10000, 0, 6, F_OATH },
    { "FIRST CONTACT",
      { "\"THE VEYR ARE REAL, AND THEY ARE HERE.",
        "HIGH COMMAND WANTS PROOF THEY CAN BE KILLED.",
        "UMBRA IS ON THE FRONT. KILL SIX. COME BACK ALIVE.\"", 0, 0 },
      "\"SIX VEYR. THEY BLEED. THEN WE CAN WIN.\"",
      MI_INTERCEPT, "UMBRA", S, 0, 6, 0, 0, 9000, 10, 8, F_VEYR },
    { "THE NAV CORE",
      { "\"A VEYR CARRIER DIED AT RHEA WHEN THEY CAME THROUGH.",
        "ITS NAV CORE IS STILL IN THE WRECK. FIND IT, AND",
        "IT WILL TELL US WHERE THEIR NEST IS.\"", 0, 0 },
      "\"WE HAVE THE PATH. THE NEBULA IS OPEN TO US.\"",
      MI_SURVEY, "RHEA", S, 0, 0, 0, 0, 12000, 10, 6, F_VEYR | F_REVEAL },
    { "OPERATION HAMMERFALL",
      { "\"TAKE VEYR PRIME. BREAK THEIR DEFENCES, ALL OF THEM.",
        "WHEN THE HIVE LIES OPEN, YOU WILL BURN IT. ALL OF IT.",
        "THOSE ARE YOUR ORDERS. HIGH COMMAND WILL NOT",
        "ACCEPT ANY OTHER OUTCOME.\"", 0 },
      "\"VEYR PRIME IS OURS. NOW FINISH IT.\"",
      MI_CAPTURE, "VEYR PRIME", P, 0, 0, 0, 0, 25000, 10, 0, F_VEYR },
};

/* ------------------------------------------------------------------------ */
/* The FSA: Major Ada Voss, FSA Command                                     */
/* ------------------------------------------------------------------------ */
static const Step fsa_steps[] = {
    { "RECRUITING POSTERS",
      { "\"YOU'RE NOT UFS. THAT'S A START.",
        "TAKE THESE TO ASHFORD. THE FREE PORT LISTENS TO US.",
        "PRINTING IS CHEAP. PILOTS ARE NOT.\"", 0, 0 },
      "\"THREE NEW PILOTS SIGNED UP IN ASHFORD.\"",
      MI_COURIER, "ASHFORD", S, 0, 0, 0, 0, 1500, 8, 14, 0 },
    { "BREAD FOR KELVIN",
      { "\"KELVIN'S HARVEST FAILED. THE UFS BLOCKADE MADE SURE",
        "NOBODY SOLD THEM SEED. TWELVE CRATES OF FOOD.",
        "THAT'S ALL. THAT'S EVERYTHING.\"", 0, 0 },
      "\"KELVIN EATS TONIGHT. THEY KNOW YOUR NAME NOW.\"",
      MI_SUPPLY, "KELVIN", P, 0, 12, 0, 0, 2500, 8, 20, 0 },
    { "THE SPOTTER",
      { "\"A UFS SPOTTER IN A TALON SITS OVER YARROW,",
        "CALLING IN STRIKES ON OUR SUPPLY RUNS.",
        "LIEUTENANT GRAYSON. MAKE HIM STOP CALLING.\"", 0, 0 },
      "\"NO MORE STRIKES ON YARROW. THANK YOU.\"",
      MI_BOUNTY, "YARROW", S, 0, 0, DEF_TALON, 1, 3500, 10, 20, 0 },
    { "ARMS FOR WICKHAM",
      { "\"WICKHAM IS A UFS TOWN SURROUNDED BY FREE SPACE.",
        "NOT EVERYONE THERE LOVES THEIR MASTERS. ARM THE ONES",
        "WHO DON'T. SIX CRATES, PAST THE UFS SCANNERS.\"", 0, 0 },
      "\"WICKHAM HAS A RESISTANCE NOW. BECAUSE OF YOU.\"",
      MI_SMUGGLE, "WICKHAM", S, COMM_STOLEN_TECH, 6, 0, 0, 4000, 10, 20, 0 },
    { "REFUGEES FROM PELL",
      { "\"UFS RAIDS BURNED HALF OF PELL. SIX FAMILIES",
        "ARE WAITING FOR A SHIP THAT WILL TAKE THEM.",
        "TAKE THEM HOME TO FORT KESTREL.\"", 0, 0 },
      "\"THE FAMILIES ARE SAFE. THE CHILDREN DREW YOUR SHIP.\"",
      MI_PASSENGER, "KESTREL", S, 0, 6, 0, 0, 4000, 8, 24, 0 },
    { "THE LOST PATROL",
      { "\"A PATROL OF OURS WENT INTO BLACK SHOAL. NONE CAME OUT.",
        "NO PORT, NO FUEL, NO REASON TO GO. GO ANYWAY.",
        "FIND OUT WHAT HAPPENED.\"", 0, 0 },
      "\"THEIR HULLS WERE... EATEN. WHAT DOES THAT?\"",
      MI_SURVEY, "BLACK SHOAL", S, 0, 0, 0, 0, 5000, 8, 24, 0 },
    { "THE PIRATE TOLL",
      { "\"PIRATES IN HESPER CHARGE A TOLL ON OUR CONVOYS.",
        "WE DON'T PAY TOLLS. FIVE OF THEM.",
        "MAKE IT A LESSON.\"", 0, 0 },
      "\"HESPER IS FREE SPACE AGAIN.\"",
      MI_INTERCEPT, "HESPER", S, 0, 5, 0, 0, 5500, 10, 24, 0 },
    { "THE FREE PRESS",
      { "\"THE TRUTH ABOUT PELL IS ON THIS DRIVE.",
        "VESTA'S NEWSFEEDS REACH HALF THE GALAXY.",
        "LET THEM SEE WHAT THE UFS IS.\"", 0, 0 },
      "\"IT'S EVERYWHERE. THE UFS IS FURIOUS. GOOD.\"",
      MI_COURIER, "VESTA", S, 0, 0, 0, 0, 6000, 10, 24, 0 },
    { "THE OLD WRECK",
      { "\"MINERS IN THE LONG DARK FOUND A WRECK OLDER THAN",
        "THE UFS. THEY WON'T GO BACK. THEY WON'T SAY WHY.",
        "SEE IT FOR ME.\"", 0, 0 },
      "\"A VEYR HULL. OUR GRANDPARENTS FOUGHT THEM. THEY'RE BACK.\"",
      MI_SURVEY, "LONG DARK", S, 0, 0, 0, 0, 7000, 8, 30, 0 },
    { "THE PLEDGE",
      { "\"YOU'VE BLED FOR US. NOW STAND WITH US.",
        "TAKE THE PLEDGE: AN FSA PILOT, FREE AND SWORN.",
        "SOMETHING IS COMING FROM THE RIM. THE UFS WILL",
        "USE IT AS AN EXCUSE. WE WON'T LET THEM.\"", 0 },
      "YOU TOOK THE PLEDGE. YOU FLY FOR THE FREE SYSTEMS.",
      0, 0, S, 0, 0, 0, 0, 10000, 0, 6, F_OATH },
    { "EVACUATION",
      { "\"THE VEYR ARE TAKING WORLDS ON THE RIM. PEOPLE ARE",
        "STRANDED ON PORTS NOBODY WILL RISK. EIGHT OF THEM",
        "ARE YOURS. GET THEM TO FORT KESTREL.\"", 0, 0 },
      "\"EIGHT LIVES. THE UFS WOULDN'T HAVE BOTHERED.\"",
      MI_PASSENGER, "KESTREL", S, 0, 8, 0, 0, 9000, 10, 8, F_VEYR },
    { "HOLD THE LINE",
      { "\"UMBRA IS THE FRONT. THE SWARMS COME THROUGH THERE.",
        "KILL EIGHT. BUY US TIME TO DIG IN.\"", 0, 0, 0 },
      "\"THE LINE HOLDS. FOR NOW.\"",
      MI_INTERCEPT, "UMBRA", S, 0, 8, 0, 0, 11000, 10, 6, F_VEYR },
    { "THE WAY IN",
      { "\"OUR SCOUTS SAY THE VEYR CAME THROUGH AT GALLOWAY.",
        "SOMETHING THERE STILL TRANSMITS. FIND IT, AND",
        "WE FIND THE WAY TO THEIR NEST.\"", 0, 0 },
      "\"THE PATH IS OPEN. THE NEST LIES PAST RHEA.\"",
      MI_SURVEY, "GALLOWAY", S, 0, 0, 0, 0, 12000, 10, 6, F_VEYR | F_REVEAL },
    { "END IT",
      { "\"TAKE VEYR PRIME. END THE WAR.",
        "WHAT YOU DO WITH THE HIVE IS YOURS TO CHOOSE.",
        "THE UFS WANTS A GENOCIDE. THE FSA WON'T ORDER ONE.",
        "BUT WE WON'T STOP YOU EITHER.\"", 0 },
      "\"VEYR PRIME IS TAKEN. THE CHOICE IS YOURS NOW.\"",
      MI_CAPTURE, "VEYR PRIME", P, 0, 0, 0, 0, 25000, 10, 0, F_VEYR },
};

/* ------------------------------------------------------------------------ */
/* The Syndicates: Old Mother Rusk, broker                                  */
/* ------------------------------------------------------------------------ */
static const Step syn_steps[] = {
    { "FIRST RUN",
      { "\"YOU HAVE AN HONEST FACE. THAT'S USEFUL, DEAR.",
        "FOUR UNITS FOR ISOLDE. THE UFS SCANS EVERYONE THERE,",
        "SO DON'T LOOK GUILTY.\"", 0, 0 },
      "\"CLEAN DELIVERY. MOTHER LIKES CLEAN.\"",
      MI_SMUGGLE, "ISOLDE", P, COMM_NARCOTICS, 4, 0, 0, 2000, 8, 14, 0 },
    { "THE WELSHER",
      { "\"SLIPPERY DEKE BORROWED FROM MOTHER AND RAN TO TALLIS.",
        "HE FLIES A WAYFARER. HE THINKS NOBODY NOTICED.",
        "WE NOTICED.\"", 0, 0 },
      "\"DEKE WON'T BE BORROWING AGAIN.\"",
      MI_BOUNTY, "TALLIS", S, 0, 0, DEF_WAYFARER, 2, 3000, 8, 20, 0 },
    { "SCRAP FOR THE YARD",
      { "\"THE YARDS AT CINDER ARE HUNGRY. TWELVE LOADS OF",
        "SCRAP. DON'T ASK WHERE IT CAME FROM, DEAR.",
        "I CERTAINLY WON'T.\"", 0, 0 },
      "\"THE TORCHES ARE BURNING AGAIN. LOVELY.\"",
      MI_CARGO, "CINDER", P, COMM_SCRAP, 12, 0, 0, 3000, 8, 20, 0 },
    { "TURF WAR",
      { "\"A CREW THAT ISN'T OURS IS WORKING BRAKE.",
        "FOUR SHIPS. TEACH THEM THE RIM HAS RULES.\"", 0, 0, 0 },
      "\"BRAKE IS OURS AGAIN. MOTHER IS PLEASED.\"",
      MI_INTERCEPT, "BRAKE", S, 0, 4, 0, 0, 4000, 10, 20, 0 },
    { "THE BIG SCORE",
      { "\"TEN UNITS OF STOLEN TECH. INTO AURORA ITSELF.",
        "THE UFS CAPITAL, DEAR. UNDER THEIR NOSES.",
        "DO THIS AND THE RIM WILL TALK ABOUT YOU.\"", 0, 0 },
      "\"INTO AURORA! THEY'LL SING SONGS ABOUT YOU.\"",
      MI_SMUGGLE, "AURORA", S, COMM_STOLEN_TECH, 10, 0, 0, 7000, 12, 24, 0 },
    { "SILENT PARTNER",
      { "\"A LETTER FOR A FRIEND IN OBERON.",
        "IT'S NOT POISONED. PROBABLY. JUST DELIVER IT.\"", 0, 0, 0 },
      "\"OUR FRIEND IN OBERON SENDS HIS REGARDS.\"",
      MI_COURIER, "OBERON", S, 0, 0, 0, 0, 4500, 8, 24, 0 },
    { "GHOST SIGNAL",
      { "\"THERE'S A SIGNAL IN THE HUSH. SOMETHING SINGING.",
        "MY BOYS WON'T GO. THEY'RE SUPERSTITIOUS.",
        "YOU'RE NOT. ARE YOU, DEAR?\"", 0, 0 },
      "\"A VEYR BEACON. STILL SINGING. THAT'S NOT GOOD.\"",
      MI_SURVEY, "THE HUSH", S, 0, 0, 0, 0, 5500, 8, 24, 0 },
    { "THE CUSTOMS MAN",
      { "\"INSPECTOR MOLL HAS BEEN SEIZING OUR CARGOES",
        "OUT OF LARKSPUR IN HIS SENTINEL. HE'S BAD FOR BUSINESS.",
        "BE WORSE FOR HIS.\"", 0, 0 },
      "\"NO MORE INSPECTIONS. THE UFS IS FURIOUS.\"",
      MI_BOUNTY, "LARKSPUR", S, 0, 0, DEF_SENTINEL, 3, 7500, 12, 24, 0 },
    { "WHAT THE MINERS FOUND",
      { "\"MINERS IN ASH VOID DUG UP SOMETHING ALIVE.",
        "THEY CALLED ME, NOT THE UFS. SMART MINERS.",
        "GO SEE WHAT IT'S WORTH.\"", 0, 0 },
      "\"VEYR TECH. LIVING TECH. WORTH EVERYTHING, DEAR.\"",
      MI_SURVEY, "ASH VOID", S, 0, 0, 0, 0, 7000, 8, 30, 0 },
    { "THE BLOOD MARK",
      { "\"YOU'VE EARNED THE MARK, DEAR. TAKE IT, AND THE RIM",
        "IS YOURS: OUR YARDS, OUR TECH, OUR PROTECTION.",
        "SOMETHING IS COMING THAT THE UFS WANTS DEAD,",
        "AND WE WANT FOR OURSELVES. BE ONE OF US.\"", 0 },
      "YOU TOOK THE BLOOD MARK. THE RIM IS FAMILY NOW.",
      0, 0, S, 0, 0, 0, 0, 10000, 0, 6, F_OATH },
    { "ALIEN SALVAGE",
      { "\"THE VEYR ARE HERE, AND THEY'RE WORTH A FORTUNE.",
        "HERE'S A CONTAINMENT RIG, ON THE HOUSE. DISABLE ONE,",
        "BOARD IT, CLAIM IT. DON'T LET IT BLOW, DEAR.\"", 0, 0 },
      "\"A LIVE VEYR HULL. MOTHER COULD KISS YOU.\"",
      MI_CLAIM, 0, S, 0, 0, 0, 0, 12000, 12, 8, F_VEYR | F_RIG },
    { "THE PATH",
      { "\"THAT HULL OF YOURS KEEPS SINGING ABOUT RHEA.",
        "THERE'S SOMETHING THERE. FIND IT, AND IT'LL",
        "SHOW US THE WAY TO WHERE THEY COME FROM.\"", 0, 0 },
      "\"THE NEST IS PAST RHEA. NOW THE REAL PRIZE.\"",
      MI_SURVEY, "RHEA", S, 0, 0, 0, 0, 12000, 10, 6, F_VEYR | F_REVEAL },
    { "THE PRIZE",
      { "\"TAKE VEYR PRIME, DEAR. TAKE THE HIVE ALIVE.",
        "THE UFS WANTS IT BURNED. WE WANT IT WORKING.",
        "DO THIS AND THE SYNDICATE WILL MAKE YOU A KING.\"", 0, 0 },
      "\"THE HIVE IS IN YOUR HANDS. KEEP IT, DEAR.\"",
      MI_CAPTURE, "VEYR PRIME", P, 0, 0, 0, 0, 25000, 15, 0, F_VEYR },
};

#undef S
#undef P

static const Step *const lines[TRACK_COUNT] = { ufs_steps, fsa_steps, syn_steps };
static const u8 line_len[TRACK_COUNT] = {
    sizeof ufs_steps / sizeof ufs_steps[0], sizeof fsa_steps / sizeof fsa_steps[0], sizeof syn_steps / sizeof syn_steps[0] };

static u8  step[TRACK_COUNT];        /* the next step to offer (or the one on your list) */
static s32 wait_until[TRACK_COUNT];  /* game hour the next orders come */
static s8  sworn;                    /* -1, or the track you swore to */
static char msg[80];

static int now_hours(void) { return (clock_day() - 1) * 24 + clock_hour(); }

void story_init(void) {
    memset(step, 0, sizeof step);
    memset(wait_until, 0, sizeof wait_until);
    sworn = -1;
#ifdef FRONTIER_HOST
    if (getenv("FR_STORY_STEP")) step[0] = step[1] = step[2] = (u8) atoi(getenv("FR_STORY_STEP"));   /* preview */
#endif
}

void story_dev_nowait(void) { memset(wait_until, 0, sizeof wait_until); }

const char *story_track_name(int t) { static const char *n[TRACK_COUNT] = { "UFS", "FSA", "SYNDICATE" }; return n[t]; }
const char *story_contact(int t) {
    static const char *n[TRACK_COUNT] = { "COMMANDER IRIS HALE, UFS NAVAL INTELLIGENCE",
                                          "MAJOR ADA VOSS, FSA COMMAND", "OLD MOTHER RUSK, SYNDICATE BROKER" };
    return n[t];
}
int story_sworn(void) { return sworn; }

static int find_sys(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < GALAXY_SYSTEMS; i++) if (strcmp(galaxy_sys(i)->name, name) == 0) return i;
    return -1;
}

/* Where a track's contact can reach you: a port of their own. */
static int contact_port(int t, int sys) {
    const StarSystem *g = galaxy_sys(sys);
    if (!galaxy_port_open(sys)) return 0;
    if (t == TRACK_UFS) return g->owner == OWN_FEDERATION;
    if (t == TRACK_FSA) return g->owner == OWN_REBEL;
    return galaxy_founder(sys) == OWN_SYNDICATE;
}

static int standing_ok(int t) {
    if (t == TRACK_UFS) return faction_rep(FAC_FEDERATION) >= 0;
    if (t == TRACK_FSA) return faction_rep(FAC_REBEL) >= 0;
    return syn_rep() > SYN_MARKED;
}

static int finished(int t) { return step[t] >= line_len[t]; }
static const Step *cur(int t) { return finished(t) ? 0 : &lines[t][step[t]]; }

const char *story_status(int t, int sys) {
    static char b[72];
    const Step *s = cur(t);
    if (sworn >= 0 && sworn != t) { snprintf(b, sizeof b, "CLOSED: YOU SWORE TO THE %s", story_track_name(sworn)); return b; }
    if (!s) return veyr_ending() ? (conquest_war_status() ? conquest_war_status() : "DONE. THE WAR IS OVER.")
                                 : "DONE: NOTHING MORE FOR NOW";
    if (missions_story_active(t)) { snprintf(b, sizeof b, "ON YOUR LIST: %s", s->title); return b; }
    if (!standing_ok(t)) return "THEY WON'T TALK TO YOU: YOUR STANDING IS TOO LOW";
    if ((s->flags & F_VEYR) && veyr_phase() == VP_DORMANT) return "WAITING. \"SOMETHING IS COMING. BE READY.\"";
    if ((s->flags & F_VEYR) && sworn != t) return "SWEAR TO THEM FIRST";
    int left = wait_until[t] - now_hours();
    if (left > 0) { snprintf(b, sizeof b, "NEW ORDERS IN %d HOURS", left); return b; }
    if (contact_port(t, sys)) { snprintf(b, sizeof b, "NEW ORDERS HERE: %s", s->title); return b; }
    static const char *where[TRACK_COUNT] = { "AT ANY UFS PORT", "AT ANY FSA PORT", "AT ANY SYNDICATE PORT" };
    snprintf(b, sizeof b, "NEW ORDERS WAITING %s", where[t]);
    return b;
}

int story_offer_here(int t, int sys) {
    const Step *s = cur(t);
    if (!s || (sworn >= 0 && sworn != t) || missions_story_active(t) || !standing_ok(t)) return 0;
    if ((s->flags & F_VEYR) && (veyr_phase() == VP_DORMANT || sworn != t)) return 0;
    if (wait_until[t] > now_hours()) return 0;
    return contact_port(t, sys);
}

const char *story_offer_title(int t) { const Step *s = cur(t); return s ? s->title : ""; }
int story_offer_lines(int t, const char **out) {
    const Step *s = cur(t);
    int n = 0;
    if (s) for (int i = 0; i < 5 && s->brief[i]; i++) out[n++] = s->brief[i];
    return n;
}
const char *story_offer_warning(int t) {
    const Step *s = cur(t);
    if (!s || !(s->flags & F_OATH)) return 0;
    static const char *w[TRACK_COUNT] = {
        "POINT OF NO RETURN: THE FSA BECOMES YOUR ENEMY, THE SYNDICATES MARK YOU, AND THEIR STORIES END.",
        "POINT OF NO RETURN: THE UFS BECOMES YOUR ENEMY, AND THE OTHER STORIES END.",
        "POINT OF NO RETURN: THE UFS BECOMES YOUR ENEMY, AND THE OTHER STORIES END." };
    return w[t];
}
int story_offer_pay(int t) { const Step *s = cur(t); return s ? s->pay : 0; }

/* Standing for a finished step: your side up, the rivals down. */
static void standing(int t, int rep) {
    if (t == TRACK_UFS) { faction_change(FAC_FEDERATION, rep); syn_change(-rep / 4); }
    else if (t == TRACK_FSA) faction_change(FAC_REBEL, rep);                 /* the UFS drops by half, by itself */
    else { syn_change(rep); faction_change(FAC_FEDERATION, -rep / 2); }
}

/* The oath: up to full trust with this side, which turns the others away (faction.c). */
static void swear(int t) {
    sworn = (s8) t;
    if (t == TRACK_UFS) {
        if (faction_rep(FAC_FEDERATION) < REP_ALLY) faction_change(FAC_FEDERATION, REP_ALLY - faction_rep(FAC_FEDERATION));
        if (syn_rep() > SYN_MARKED) syn_set(SYN_MARKED);
    } else if (t == TRACK_FSA) {
        if (faction_rep(FAC_REBEL) < REP_ALLY) faction_change(FAC_REBEL, REP_ALLY - faction_rep(FAC_REBEL));
        if (faction_rep(FAC_FEDERATION) > -40) faction_set(FAC_FEDERATION, -40);
    } else {
        int need = syn_level_needs(SYN_ESTABLISHED);
        if (syn_rep() < need) syn_change(need - syn_rep());
        if (faction_rep(FAC_FEDERATION) > -40) faction_set(FAC_FEDERATION, -40);
    }
}

const char *story_accept(int t, int sys, int nav, int cargo_cap) {
    if (!story_offer_here(t, sys)) return "NOTHING TO TAKE HERE";
    const Step *s = cur(t);
    if (s->flags & F_OATH) {
        swear(t);
        credits += s->pay;
        step[t]++;
        wait_until[t] = now_hours() + s->wait;
        snprintf(msg, sizeof msg, "%s +%d CR", s->done, s->pay);
        veyr_banner(t == TRACK_UFS ? "YOU ARE UFS NOW" : t == TRACK_FSA ? "YOU ARE FSA NOW" : "YOU ARE SYNDICATE NOW",
                    s->done, "THERE IS NO GOING BACK.");
        return msg;
    }
    Mission m = { 0 };
    m.type = s->type;
    m.giver = (u8) (t == TRACK_UFS ? GIVER_UFS : t == TRACK_FSA ? GIVER_FSA : GIVER_SYN);
    int d = find_sys(s->dest);
    m.dest_sys = (u8) (d >= 0 ? d : sys);
    m.dest_nav = s->nav;
    m.commodity = s->commodity;
    m.units = s->units;
    m.target_def = s->target_def;
    m.name = (u8) (8 + s->wanted);
    m.reward = s->pay;
    m.deadline = 0;
    m.story = (u8) (t + 1);
    (void) nav;
    const char *r = missions_accept_custom(&m, cargo_cap);
    if (!missions_story_active(t)) return r;          /* no room on the list, or in the hold */
    if ((s->flags & F_RIG) && veyr_contain() < 1) veyr_contain_set(1);
    snprintf(msg, sizeof msg, "%s: ON YOUR CONTRACT LIST", s->title);
    return msg;
}

const char *story_completed(int t) {
    const Step *s = cur(t);
    if (!s) return 0;
    standing(t, s->rep);
    if (s->flags & F_REVEAL) {
        static const char *who[TRACK_COUNT] = { "UFS NAVAL INTELLIGENCE", "FSA SCOUTS", "THE SYNDICATE" };
        veyr_reveal(who[t]);
    }
    step[t]++;
    wait_until[t] = now_hours() + s->wait;
    static char head[40], l2[64];
    snprintf(head, sizeof head, "%s: %s", story_track_name(t), s->title);
    if (finished(t)) snprintf(l2, sizeof l2, "+%d CR. THAT WAS THE LAST OF THEIR ORDERS.", s->pay);
    else snprintf(l2, sizeof l2, "+%d CR. NEW ORDERS IN %d HOURS (DOCK: COMMS).", s->pay, s->wait);
    veyr_banner(head, s->done, l2);
    snprintf(msg, sizeof msg, "%s DONE: PAID %d", s->title, s->pay);
    return msg;
}

void story_failed(int t) {
    (void) t;                     /* the step stays where it is: the contact offers it again */
}

u8 *story_ser(u8 *p, int load) {
    if (load && ser_version < 11) { story_init(); return p; }
    SER(p, load, step); SER(p, load, wait_until); SER(p, load, sworn);
    return p;
}
