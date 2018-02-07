#include "routeur.h"
#include "bsp_init.h"
#include "platform.h"
#include <stdlib.h>
#include <stdbool.h>
#include <xil_printf.h>
#include <xgpio.h>

///////////////////////////////////////////////////////////////////////////////////////
//								uC-OS global variables
///////////////////////////////////////////////////////////////////////////////////////
#define TASK_STK_SIZE 8192

///////////////////////////////////////////////////////////////////////////////////////
//								Routines d'interruptions
///////////////////////////////////////////////////////////////////////////////////////

void timer_isr(void* not_valid) {
	if (private_timer_irq_triggered()) {
		private_timer_clear_irq();
		OSTimeTick();
	}
}

void fit_timer_1s_isr(void *not_valid) {
	/* À compléter */
}

void fit_timer_3s_isr(void *not_valid) {
	/* À compléter */
}

void gpio_isr(void * not_valid) {
	/* À compléter */
}

/*
 *********************************************************************************************************
 *                                            computeCRC
 * -Calcule la check value d'un pointeur quelconque (cyclic redudancy check)
 * -Retourne 0 si le CRC est correct, une autre valeur sinon.
 *********************************************************************************************************
 */
unsigned int computeCRC(uint16_t* w, int nleft) {
	unsigned int sum = 0;
	uint16_t answer = 0;

	// Adding words of 16 bits
	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	// Handling the last byte
	if (nleft == 1) {
		*(unsigned char *) (&answer) = *(const unsigned char *) w;
		sum += answer;
	}

	// Handling overflow
	sum = (sum & 0xffff) + (sum >> 16);
	sum += (sum >> 16);

	answer = ~sum;
	return (unsigned int) answer;
}

/*
 *********************************************************************************************************
 *                                          computePacketCRC
 * -Calcule la check value d'un paquet en utilisant un CRC (cyclic redudancy check)
 * -Retourne 0 si le CRC est correct, une autre valeur sinon.
 *********************************************************************************************************
 */
static inline unsigned int computePacketCRC(Packet* packet) {
	return computeCRC((uint16_t*) packet, sizeof(Packet));
}
///////////////////////////////////////////////////////////////////////////////////////
//								uC/OS-II part
///////////////////////////////////////////////////////////////////////////////////////
int main() {

	initialize_bsp();

	// Initialize uC/OS-II
	OSInit();

	create_application();

	prepare_and_enable_irq();

	xil_printf("*** Starting uC/OS-II scheduler ***\n");

	OSStart();

	mode_profilage = 0;

	cleanup();
	cleanup_platform();

	return 0;
}

void create_application() {
	int error;

	error = create_tasks();
	if (error != 0)
		xil_printf("Error %d while creating tasks\n", error);

	error = create_events();
	if (error != 0)
		xil_printf("Error %d while creating events\n", error);
}

int create_tasks() {
	// Stacks
	static OS_STK TaskReceiveStk[TASK_STK_SIZE];
	static OS_STK TaskVerifySourceStk[TASK_STK_SIZE];
	static OS_STK TaskVerifyCRCStk[TASK_STK_SIZE];
	static OS_STK TaskStatsStk[TASK_STK_SIZE];
	static OS_STK TaskComputeStk[TASK_STK_SIZE];
	static OS_STK TaskForwardingStk[TASK_STK_SIZE];
	static OS_STK TaskPrint1Stk[TASK_STK_SIZE];
	static OS_STK TaskPrint2Stk[TASK_STK_SIZE];
	static OS_STK TaskPrint3Stk[TASK_STK_SIZE];

	/* À compléter */

	return 0;
}

int create_events() {
	uint8_t err;

	static void* inputMsg[1024];
	static void* lowMsg[1024];
	static void* mediumMsg[1024];
	static void* highMsg[1024];

	/* À compléter: création des files, mailbox, sémaphores et mutex */

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
//								uC/OS-II part
///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////
//									TASKS
///////////////////////////////////////////////////////////////////////////////////////

/*
 *********************************************************************************************************
 *											  TaskGeneratePacket
 *  - Génère des paquets et les envoie dans la InputQ.
 *  - À des fins de développement de votre application, vous pouvez *temporairement* modifier la variable
 *    "shouldSlowthingsDown" à  true pour ne générer que quelques paquets par seconde, et ainsi pouvoir
 *    déboguer le flot de vos paquets de manière plus saine d'esprit. Cependant, la correction sera effectuée
 *    avec cette variable Ã  false.
 *********************************************************************************************************
 */
void TaskGeneratePacket(void *data) {
	srand(42);
	uint8_t err;
	bool isGenPhase = true; 					// Indique si on est dans la phase de generation ou non
	const bool shouldSlowThingsDown = true;		// Variable à modifier
	int packGenQty = (rand() % 250);
	while (true) {
		if (isGenPhase) {
			Packet *packet = malloc(sizeof(Packet));

			packet->src = rand() * (UINT32_MAX / RAND_MAX);
			packet->dst = rand() * (UINT32_MAX / RAND_MAX);
			packet->type = rand() % NB_PACKET_TYPE;

			//Compute CRC
			packet->crc = 0;
			if (rand() % 10 == 9) // 10% of Packets with bad CRC
				packet->crc = 1234;
			else
				packet->crc = computeCRC((unsigned short*) (&packet), 64);

			for (int i = 0; i < ARRAY_SIZE(packet->data); ++i)
				packet->data[i] = (unsigned int) rand();
			packet->data[0] = nbPacketCrees;

			nbPacketCrees++;

			if (shouldSlowThingsDown) {
				xil_printf("GENERATE : ********GÃ©nÃ©ration du Paquet # %d ******** \n", nbPacketCrees);
				xil_printf("ADD %x \n", packet);
				xil_printf("	** src : %x \n", packet->src);
				xil_printf("	** dst : %x \n", packet->dst);
				xil_printf("	** crc : %x \n", packet->crc);
				xil_printf("	** type : %d \n", packet->type);
			}

			err = OSQPost(inputQ, packet);

			if (err == OS_ERR_Q_FULL) {
				xil_printf(
						"GENERATE: Paquet rejeté a l'entrée car la FIFO est pleine !\n");
				free(packet);
			}

			if (shouldSlowThingsDown) {
				OSTimeDlyHMSM(0, 0, 0, 200 + rand() % 600);
			} else {
				OSTimeDlyHMSM(0, 0, 0, 2);

				if ((nbPacketCrees % packGenQty) == 0) //On génère jusqu'à 250 paquets par phase de génération
						{
					isGenPhase = false;
				}
			}
		} else {
			OSTimeDlyHMSM(0, 0, 0, 500);
			isGenPhase = true;
			packGenQty = (rand() % 250);
			xil_printf(
					"GENERATE: Génération de %d paquets durant les %d prochaines millisecondes\n",
					packGenQty, packGenQty * 2);
		}
	}
}

/*
 *********************************************************************************************************
 *											  TaskVerifySource
 *  -Stoppe le routeur une fois que 200 paquets ont été rejetés pour mauvaise source
 *  -Ne doit pas stopper la tâche d'affichage des statistiques.
 *********************************************************************************************************
 */
void TaskVerifySource(void *data) {
	uint8_t err;
	while (true) {
		/* À compléter */
	}
}

/*
 *********************************************************************************************************
 *											  TaskVerifyCRC
 *  -Stoppe le routeur une fois que 200 paquets ont été rejetés pour mauvais CRC
 *  -Ne doit pas stopper la tâche d'affichage des statistiques.
 *********************************************************************************************************
 */
void TaskVerifyCRC(void *data) {
	uint8_t err;
	while (true) {
		/* À compléter */
	}
}

/*
 *********************************************************************************************************
 *											  TaskComputing
 *  -Vérifie si les paquets sont conformes (CRC,Adresse Source)
 *  -Dispatche les paquets dans des files (HIGH,MEDIUM,LOW)
 *
 *********************************************************************************************************
 */
void TaskComputing(void *pdata) {
	uint8_t err;
	Packet *packet = NULL;
	while (true) {
		/* À compléter */
	}
}

/*
 *********************************************************************************************************
 *											  TaskForwarding
 *  -Traite la priorité des paquets : si un paquet de haute priorité est prêt,
 *   on l'envoie à l'aide de la fonction dispatch, sinon on regarde les paquets de moins haute priorité
 *********************************************************************************************************
 */
void TaskForwarding(void *pdata) {
	uint8_t err;
	Packet *packet = NULL;
	while (true) {
		/* À compléter */
	}
}

/*
 *********************************************************************************************************
 *                                              TaskStats
 *  -Est déclenchée lorsque le gpio_isr() libère le sémpahore
 *  -Si en mode profilage, calcule les statistiques des files
 *  -En sortant de la période de profilage, affiche les statistiques des files et du routeur.
 *********************************************************************************************************
 */
void TaskStats(void *pdata) {
	uint8_t err;
	while (true) {
		/* À compléter */

		xil_printf("\n------------------ Affichage des statistiques ------------------\n");
		xil_printf("Nb de packets total traites : %d\n", nbPacketCrees);
		xil_printf("Nb de packets total traites : %d\n", nbPacketTraites);
		xil_printf("Nb de packets rejetes pour mauvaise source : %d\n",	nbPacketSourceRejete);
		xil_printf("Nb de packets rejetes pour mauvais crc : %d\n",	nbPacketCRCRejete);

		xil_printf("Nb d'echantillons de la période de profilage : %d\n", nb_echantillons);
		xil_printf("Maximum file input : %d\n", max_msg_input);
		xil_printf("Moyenne file input : %d\n", moyenne_msg_input);
		xil_printf("Maximum file low : %d\n", max_msg_low);
		xil_printf("Moyenne file low : %d\n", moyenne_msg_low);
		xil_printf("Maximum file medium : %d\n", max_msg_medium);
		xil_printf("Moyenne file medium : %d\n", moyenne_msg_medium);
		xil_printf("Maximum file high : %d\n", max_msg_high);
		xil_printf("Moyenne file high : %d\n", moyenne_msg_high);

		/* À compléter */
	}
}

/*
 *********************************************************************************************************
 *											  TaskPrint
 *  -Affiche les infos des paquets arrivés Ã  destination et libere la mémoire allouée
 *********************************************************************************************************
 */
void TaskPrint(void *data) {
	uint8_t err;
	Packet *packet = NULL;
	int intID = ((PRINT_PARAM*) data)->interfaceID;
	OS_EVENT* mb = ((PRINT_PARAM*) data)->Mbox;

	while (true) {
		/* À compléter */
	}

}

void err_msg(char* entete, uint8_t err) {
	if (err != 0) {
		xil_printf(entete);
		xil_printf(": Une erreur est retournée : code %d \n", err);
	}
}
