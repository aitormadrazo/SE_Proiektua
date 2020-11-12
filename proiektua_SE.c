#define DEBUG 

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include "pcb_ilara.c"



/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////Egitura///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////


struct tikak_konpartitua_struct{
    int tikak;
    pthread_mutex_t mutex; // Mutex aldagaia
};





struct erloju_parametroak_struct {
    int maiztasuna; //Maiztasuna = zenbat microsegundu tick artean
};
struct timer_parametroak_struct {
    int timer_maiztazuna;//erloju ziklotan neurtua
};

struct process_generator_parametroak_struct {
    int process_maiztasuna;//erloju ziklotan neurtua
};

struct core_parametroak_struct {
    int core_id;
};

struct cpu_struct {
    int core_kop;
    pthread_t *coreHariak;
    int quantum; //erloju ziklotan neurtua
    int erloju_maiztasuna;
    struct Queue* *coreIlarak;



};
/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////Aldagai Globalak///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

static sem_t sartu; // Hariak zenbakia sartzeko semaforo aldagaia
static sem_t atera; // Azken hariak sartutako zenbakia irakurtzeko semaforo aldagaia

static struct tikak_konpartitua_struct tikak_konp;
static struct Queue* pcb_ilara;

static struct cpu_struct cpu;
static int sartzeIndizea;




/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////erlojua///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

void *erlojua(void *parametroak){
struct erloju_parametroak_struct *param = (struct erloju_parametroak_struct *)parametroak;

while(1){
  pthread_mutex_lock(&tikak_konp.mutex);
	tikak_konp.tikak= tikak_konp.tikak + 1;
  pthread_mutex_unlock(&tikak_konp.mutex);
  //printf("TICK \n");
  usleep(param->maiztasuna);

}

return 0;

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////scheduler_etena///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

void scheduler_etena(){
  struct pcb_struct ateratakoa;


//pthread_mutex_lock(&tikak_konp.mutex);


  while(!isEmpty(pcb_ilara)){

      pthread_mutex_lock(&pcb_ilara->mutex);
      ateratakoa =dequeue(pcb_ilara);
      pthread_mutex_unlock(&pcb_ilara->mutex);

      pthread_mutex_lock(&cpu.coreIlarak[sartzeIndizea]->mutex);
      enqueue(cpu.coreIlarak[sartzeIndizea], ateratakoa);
      pthread_mutex_unlock(&cpu.coreIlarak[sartzeIndizea]->mutex);

      sartzeIndizea=sartzeIndizea + 1;
      if (sartzeIndizea>=cpu.core_kop) {
        sartzeIndizea=0;
      }


  }
  printf("SCHEDULER: Planifikazioa egin dut. Uneko tamainak: \n");
  int i;
  for(i = 0; i < cpu.core_kop; i++){

    printf("Core: %d\tKop: %d\n",i, kopurua(cpu.coreIlarak[i]));


  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////timer///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

void *timer(void *parametroak){
  struct timer_parametroak_struct *param = (struct timer_parametroak_struct *)parametroak;
    while(1){
      pthread_mutex_lock(&tikak_konp.mutex);
    	if(tikak_konp.tikak >= param->timer_maiztazuna){

        tikak_konp.tikak=0;
    		printf("TIMER \n");
        scheduler_etena();
    	}
      pthread_mutex_unlock(&tikak_konp.mutex);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////process_generator///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

void *process_generator(void *parametroak){

  struct process_generator_parametroak_struct *param = (struct process_generator_parametroak_struct *)parametroak;
  while(1){

      usleep(param->process_maiztasuna*cpu.erloju_maiztasuna);
      struct pcb_struct pcb;
      pcb.pid++;
      pcb.lehentasuna=rand() % 100;
      pcb.denbora=(rand() % 10000) ; //erloju ziklotan neurtua

      pthread_mutex_lock(&pcb_ilara->mutex);
      enqueue(pcb_ilara, pcb);
      pthread_mutex_unlock(&pcb_ilara->mutex);
      # ifdef DEBUG
      printf("procesua sortu dut.pid=  %d  procesu denbora %d \n", pcb.pid, pcb.denbora);
      printf("prozesu kopurua: %d \n", pcb_ilara->size);
      # endif
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////core_funtzioa///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

void *core_funtzioa(void *parametroak){
  struct pcb_struct ateratakoa;
  int hutsa_dago=1;
  struct core_parametroak_struct *param = (struct core_parametroak_struct *)parametroak;
  # ifdef DEBUG
  printf("Core bat sortu da: %d \n", param->core_id);
  # endif

  while (1) {
    pthread_mutex_lock(&cpu.coreIlarak[param->core_id]->mutex);
    if(isEmpty(cpu.coreIlarak[param->core_id])){
      hutsa_dago=1;
    }else{
      ateratakoa =dequeue(cpu.coreIlarak[param->core_id]);
      hutsa_dago=0;
    }
    pthread_mutex_unlock(&cpu.coreIlarak[param->core_id]->mutex);

    if(hutsa_dago==0){
    if(ateratakoa.denbora<=cpu.quantum){

      usleep(ateratakoa.denbora*cpu.erloju_maiztasuna);
      # ifdef DEBUG
      printf("Core[%d]: prozesu bat amaitu dut  \n", param->core_id);
      # endif
    }
    else{

      usleep(cpu.quantum*cpu.erloju_maiztasuna);
      ateratakoa.denbora=ateratakoa.denbora - cpu.quantum;

      pthread_mutex_lock(&cpu.coreIlarak[param->core_id]->mutex);
      enqueue(cpu.coreIlarak[param->core_id], ateratakoa);
      pthread_mutex_unlock(&cpu.coreIlarak[param->core_id]->mutex);

    }
    }
  }





}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////main///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char const *argv[]) {
  int i, err;

  sartzeIndizea=0;

  pcb_ilara = createQueue(1000);

  //int maiztasuna;
  tikak_konp.tikak=0;
  char *p;
  if(argc !=6){
      fprintf(stderr, "Erabilpena: proiektu <erloju maiztasuna> <timer maiztasuna> <prozesu sorketa maiztasuna> <core_kop> <quantuma>\n");
      exit(1);
  }



  pthread_t erlojuHaria;
  pthread_t timerHaria;
  pthread_t processHaria;

  struct erloju_parametroak_struct erloju_parametroak;
  struct timer_parametroak_struct timer_parametroak;
  struct process_generator_parametroak_struct process_parametroak;


  erloju_parametroak.maiztasuna = strtol(argv[1], &p, 10); // Lehen argumentua irakurri
  timer_parametroak.timer_maiztazuna = strtol(argv[2], &p, 10); // Bigarren argumentua irakurri
  process_parametroak.process_maiztasuna = strtol(argv[3], &p, 10); // Hirugarren argumentua irakurri
  cpu.core_kop=strtol(argv[4], &p, 10); //Laugarren argumentua irakurri
  cpu.quantum=strtol(argv[5], &p, 10);
  cpu.erloju_maiztasuna = erloju_parametroak.maiztasuna;

  cpu.coreIlarak = malloc(cpu.core_kop * sizeof(struct Queue*));

  for(i = 0;i < cpu.core_kop;i++) {
    cpu.coreIlarak[i] = createQueue(1000);
  }



  pthread_create(&erlojuHaria, NULL, erlojua, (void *)&erloju_parametroak);
  pthread_create(&timerHaria, NULL, timer, (void *)&timer_parametroak);
  pthread_create(&processHaria, NULL, process_generator, (void *)&process_parametroak);

  /////////////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////CPU_Hariak///////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////////


  struct core_parametroak_struct *parametroak;

  cpu.coreHariak = malloc(cpu.core_kop * sizeof(pthread_t));
  parametroak = malloc(cpu.core_kop * sizeof(struct core_parametroak_struct));

  for(i = 0; i < cpu.core_kop; i++){
      parametroak[i].core_id = i;
      err = pthread_create(&cpu.coreHariak[i], NULL, core_funtzioa, (void *)&parametroak[i]);;

      if(err > 0){
          fprintf(stderr, "Errore bat gertatu da core haria sortzean.\n");
          exit(1);
      }
  }

  for(i = 0;i < cpu.core_kop;i++) {
    pthread_join(cpu.coreHariak[i], NULL);
  }


  free(cpu.coreHariak);
  free(parametroak);






pthread_join(erlojuHaria, NULL);
pthread_join(timerHaria, NULL);
pthread_join(processHaria, NULL);


return 0;

}