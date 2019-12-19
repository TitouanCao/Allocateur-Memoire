#include "mem.h"
#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// constante définie dans gcc seulement
#ifdef __BIGGEST_ALIGNMENT__
#define ALIGNMENT __BIGGEST_ALIGNMENT__
#else
#define ALIGNMENT 16
#endif

/* La seule variable globale autorisée
 * On trouve à cette adresse la taille de la zone mémoire
 */
static void* memory_addr;

static inline void *get_system_memory_adr() {
	return memory_addr;
}

static inline size_t get_system_memory_size() {
	return *(size_t*)memory_addr;
}

static mem_fit_function_t *mem_fit_fn;
struct ib {			//Structure du bloc de tête
	size_t size;
	struct fb* next;
	mem_fit_function_t* f;		//Méthode de recherche de mémoire
};

struct fb {			//Structure des blocs libres
	size_t size;
	struct fb* next;
};


void mem_init(void* mem, size_t taille){						//Initialisation de la structure de blocs
	memory_addr = mem;				
    *(size_t*)memory_addr = taille;
	assert(mem == get_system_memory_adr());		
	assert(taille == get_system_memory_size());
	
	struct ib* init = (struct ib*) (mem);						//Création d'un bloc de tête
	struct fb* pfb = (struct fb*) (mem+sizeof(struct ib));		//Création d'un premier bloc libre
	init->size = taille+sizeof(struct ib);						//Attribution de la taille du bloc total
	init->next = pfb;											//Attache du premier bloc au bloc tête
	init->f = mem_fit_fn;										//Attribution de la fonction de déterminisation de la manière de recherche
	pfb->size = taille;											//Atribution de la taille de premier bloc
	pfb->next = NULL;											//Le premier bloc n'a pas de suivant
	mem_fit(&mem_fit_first);									//Définition de la fonction de recher à fit first
}struct fb init;

void mem_show(void (*print)(void *, size_t, int)) {				//Affichage des blocs de mémoire
	struct fb *b = ((struct ib*)memory_addr)->next;				//Récupération du premier bloc
	if (b == NULL) {		//Message par défaut si il n'y a pas de place disponible dans la mémoire
		print((void *)((memory_addr) + sizeof(struct ib) + sizeof(size_t)) , ((struct ib*)memory_addr)->size - sizeof(struct ib) - sizeof(size_t), 0);
	}
	else {
		print((void*) ((size_t)b + sizeof(size_t)), b->size - sizeof(size_t), 1);										//Affichage du premier bloc
		if ((size_t) memory_addr + sizeof(struct ib) - (size_t)b > 0) {		//Si il existe un expace entre la tête et le premier bloc (libre) cela veut dire qu'il y a un bloc occupé entre les deux
			print((void *)((size_t) memory_addr + sizeof(struct ib))+sizeof(size_t), (size_t) b - ((size_t) memory_addr + sizeof(struct ib)) - sizeof(size_t), 0);		//Affichage de ce bloc occupé
		}

		while (b != NULL && b->next != NULL) {			//Si il existe d'autres blocs
			if ((size_t) b + b->size < (size_t) b->next) {		//Si il y a un espace de mémoire entre la fin du bloc libre courant et le début du bloc suivant cela veut dire qu'il y a un bloc occupé entre les deux
				print((void *)((size_t) b + b->size +sizeof(size_t)), ((size_t) b->next - ((size_t)b + b->size)) - sizeof(size_t), 0);	//On l'affiche
			} 
			b = b->next;		//On passe au bloc libre suivant
			print((void*) ((size_t)b + sizeof(size_t)), b->size - sizeof(size_t), 1);	//On affiche ce nouveau bloc
		}	
	}
}
void mem_fit(mem_fit_function_t *f) {		//Fonction de définition de la fonction de recherche
	mem_fit_fn=f;
}

int set_previous_free(struct fb* new_free, size_t previous_free) {		//Fixe le champs next du bloc libre précédent de l'adresse passée en deuxième paramètre au bloc libre passé en premier paramètre (sert à reformer la chaîne de blocs)
	struct fb* list = ((struct ib*)memory_addr)->next;					//Récupération du premier bloc libre
	if (list == NULL) {			//Si toute la mémoire est occupée il n'y a aucun bloc vide, nous ne sommes pas supposés arriver ici
		printf("Aucune mémoire disponible\n");
		return -1;
	}

	if (list->next == NULL || (size_t)list == previous_free) {			//Si il est seul
		((struct ib*)memory_addr)->next = new_free;						//Le bloc tête pointe vers le bloc libre passé en paramètre
		return 0;
	}
	while (list != NULL && list->next != NULL) {						//Tant qu'il reste des blocs libres
		if ((size_t)(list->next) == previous_free) {					//Si le champs next du bloc courant est l'adresse passée en paramètre
			list->next = new_free;										//Ce bloc pointe maintenant vers le bloc passé en paramètre
			return 1;
		}
		list = list->next;
	}
	return -1;
}

struct fb* get_last_free() {				//Récupère le dernier bloc libre
	struct fb* list = ((struct ib*)memory_addr)->next;	
	while (list != NULL && list->next != NULL) {
		list = list->next;
	}
	return list;
}

void *mem_alloc(size_t taille) {		//Alloue un bloc qui propose une taille au moins de la taille passée en paramètre
	if (taille <= 0) return NULL;
	struct fb *afb = mem_fit_fn(((struct ib*)memory_addr)->next, taille+sizeof(size_t));	//Création d'un bloc libre à partir de la fonction de recherche définie auparavant
	if (afb == NULL) {																		//Echec de la fonction de recherche
		printf("La taille demandée dépasse la capacité\n");
		return NULL;
		}
	size_t size = afb->size;
	afb->size = taille + sizeof(size_t);

	if (taille%8 != 0) {				//On ne donne que des blocs de mémoires multiples de 8, si ce n'est pas le cas de la demande on arrondit au multiple supérieur
		afb->size = afb->size + (8 - (taille%8));
	}
	
	if (((size_t)afb->next - ((size_t)afb + taille)) < (sizeof(size_t) + 8)) {	//Si il reste un morceau de mémoire entre le nouveau bloc et le bloc qui le suit, et que ce morceau est trop petit pour recevoir un bloc
		afb->size = taille + ((size_t)afb->next - ((size_t)afb + taille));		//On l'ajoute au bloc courant
	}	
	
	int check;
	if (size - afb->size > sizeof(size_t)) {						//Si le bloc libre restant a une taille supérieure à 8
		struct fb *nfb = (struct fb*) ((size_t)afb + afb->size);	//Le nouveau bloc libre (dû à la coupure du bloc libre en un bloc occupé et un bloc libre)
		nfb->size = size - afb->size;
		nfb->next = afb->next;
		check = set_previous_free(nfb, (size_t) afb);				//On relie le précédent avec ce block
	}
	else {													 //Si le bloc libre restant a une taille de 0 et est donc inutile
		check = set_previous_free(afb->next, (size_t) afb);  //On relie le précédent directement avec le suivant 
	}

	if (check == -1) {									 	//Rien n'a été trouvé, on a travaillé avec des objets vides
		printf("Unexpected error\n");
		return NULL;
	}
	return (void *) ((size_t)afb+sizeof(size_t));			//Renvoie de l'adresse du bloc occupé utilisable
}

void mem_free(void* mem) {				//Libération d'un bloc occupé
	mem+=120;							//Problème inconnu, les adresses sont décalées de 120 entre l'allocation et le free
	struct fb* first = ((struct ib*)memory_addr)->next;		//Premier bloc, puis bloc courant
	struct fb* prec = NULL;									//Bloc précédent, si il y en a un
	struct fb* block = (struct fb*) mem-sizeof(size_t);		//Bloc à libérer
	if (first == NULL) {
		((struct ib*)memory_addr)->next = block;
	}
	else {
		while ((size_t) first < (size_t) block && first->next != NULL) {		//On cherche le bloc qui suit notre bloc à libérer
			prec = first;				//Dès qu'un bloc est avant celui à libérer on le passe mais on garde le précédent pour pouvoir le relier avec celui à libérer et ainsi l'intégrer comme maillon de la chaîne
			first = first->next;
		} 

		block->next = first;
		if (prec == NULL) {			//Pas de précédent, cela veut dire que le précédent est la tête
			((struct ib*)memory_addr)->next = block;		//La tête prend le bloc à libérer comme suivant
			if ((size_t)block + block->size == (size_t) first) {	//Si le bloc libre suivant est apposé
				block->size = block->size + first->size;			//On les fusionne
				block->next = first->next;	
			}
		}
		else {					//Si on a un précédent
			prec->next = block;	//Il prend le bloc à libérer comme suivant
			if ((size_t)prec + prec->size == (size_t) block) {	//Si il est directement apposé (le précédent)
				prec->size = prec->size + block->size;			//On le fusionne
				prec->next = block->next;
				if ((size_t)prec + prec->size == (size_t) first) {	//Si le bloc ainsi formé est aussi apposé au bloc suivant
					prec->size = prec->size + first->size;			//On les fusionne
					prec->next = first->next;
				}
			}
			else if ((size_t)block + block->size == (size_t) first) {	//Si seulement le suivant est apposé
				block->size = block->size + first->size;				//On les fusionne
				block->next = first->next;
			}
		}
	}
}


struct fb* mem_fit_first(struct fb *list, size_t size) {	//Renvoie l'adresse du premier bloc libre qui fournit assez de mémoire
	if (list != NULL) {
		while (list != NULL && list->size < size) {		//Dès que le bloc courant a un taille suffisante on le prend, le +8 permet d'éviter de ne plus avoir de bloc libre
			list = list->next;
		}
	}
	return list;
}

/* Fonction à faire dans un second temps
 * - utilisée par realloc() dans malloc_stub.c
 * - nécessaire pour remplacer l'allocateur de la libc
 * - donc nécessaire pour 'make test_ls'
 * Lire malloc_stub.c pour comprendre son utilisation
 * (ou en discuter avec l'enseignant)
 */
size_t mem_get_size(void *zone) {
	struct fb* bloc = (struct fb*) zone-sizeof(size_t);
	return bloc->size;
}

/* Fonctions facultatives
 * autres stratégies d'allocation
 */
struct fb* mem_fit_best(struct fb *list, size_t size) {
	int bestRatio = list->size-size;	//Représente la mémoire non utilisée du bloc pris la plus petite
	struct fb* bestFreeBlock = list;	//Le bloc qui auquel la valeur ci-dessus correspond
	int currentLoss;					//La valeur de mémoire non utilisée pour chaque bloc
	while (list != NULL) {
		while (list != NULL && list->size < size) {			//On parcourt les blocs
			list = list->next;
		}
		if (list == NULL) { break; }		//On s'arrête après avoir parcouru tous les blocs
		currentLoss = list->size-size;		//On calcul la partie de la mémoire non utilisée
		if (bestRatio > currentLoss) {		//Si celle-ci est plus petite que notre meilleur cas (la plus petite perte jusqu'ici)
			bestRatio = currentLoss;		//Elle devient la plus petite perte
			bestFreeBlock = list;			//Et on associe le bloc correspondant
		}
		list = list->next;					
	}
	return bestFreeBlock;		//On renvoit le bloc trouvé comme le plus intéressant pour ne pas prendre un trop petit espace sur un gros bloc
}

struct fb* mem_fit_worst(struct fb *list, size_t size) {	//Même chose que la fonction précédente mais inversé
	int worstRatio = list->size-size;
	struct fb* worstFreeBlock = list;
	int currentLoss;
	while (list != NULL) {
		while (list != NULL && list->size < size) {		
			list = list->next;
		}
		if (list == NULL) { break; }
		currentLoss = list->size-size;
		if (worstRatio < currentLoss) {
			worstRatio = currentLoss;
			worstFreeBlock = list;
		}
		list = list->next;
	}
	return worstFreeBlock;
}
