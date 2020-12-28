
#define MEMORIA_ERRESERBATU_TAMAINA 10000
#define MEMORIA_TAMAINA 16777216
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include "pcb_ilara.c"
#include <stdint.h>


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

struct process_loader_parametroak_struct {
    int process_maiztasuna;//erloju ziklotan neurtua
};

struct core_parametroak_struct {
    int core_id;
};
struct TLB_struct {
  //egiteko
};


struct MMU_struct {
    struct TLB_struct TLB;
};
struct coreHariak_struct {
    pthread_t pthreada;
    int PC;
    int IR;
    int PTBR;
    int erregistroak[16];// = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    struct MMU_struct MMU;
};

struct cpu_struct {
    int core_kop;
    //pthread_t *coreHariak; //struct  pthread_t *coreHariak;
    struct coreHariak_struct  *coreHariak;
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

static uint8_t* memoria_fisikoa;
static uint8_t* memoria_fisikoa_egoera;





/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////Funtzio lagungarriak///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

void idatziMemoriaFisikoan32(uint32_t helbidea, uint32_t balioa )
{

        memoria_fisikoa[helbidea] = (balioa & 0xff000000) >> 24;
        memoria_fisikoa[helbidea+1]= (balioa & 0x00ff0000) >> 16;
        memoria_fisikoa[helbidea+2] = (balioa & 0x0000ff00) >>  8;
        memoria_fisikoa[helbidea+3] = (balioa & 0x000000ff)      ;
}


uint32_t irakurriMemoriaFisikotik32(uint32_t helbidea)
{
        uint32_t balioa;

        balioa  = memoria_fisikoa[helbidea] << 24;
        balioa |=   memoria_fisikoa[helbidea+1] << 16;
        balioa |= memoria_fisikoa[helbidea+2] <<  8;
        balioa |= memoria_fisikoa[helbidea+3]      ;

        return balioa;
}


void idatziMemoriaEgoeran32(uint32_t helbidea, uint32_t balioa )
{

        memoria_fisikoa_egoera[helbidea] = (balioa & 0xff000000) >> 24;
        memoria_fisikoa_egoera[helbidea+1]= (balioa & 0x00ff0000) >> 16;
        memoria_fisikoa_egoera[helbidea+2] = (balioa & 0x0000ff00) >>  8;
        memoria_fisikoa_egoera[helbidea+3] = (balioa & 0x000000ff)      ;
}


uint32_t irakurriMemoriaEgoeratik32(uint32_t helbidea)
{
        uint32_t balioa;

        balioa  = memoria_fisikoa_egoera[helbidea] << 24;
        balioa |=   memoria_fisikoa_egoera[helbidea+1] << 16;
        balioa |= memoria_fisikoa_egoera[helbidea+2] <<  8;
        balioa |= memoria_fisikoa_egoera[helbidea+3]      ;

        return balioa;
}



uint32_t helbideFisikoaKalkulatu(uint32_t pageTableHelbidea, uint32_t helbideBirtuala){
  return irakurriMemoriaFisikotik32(pageTableHelbidea) + helbideBirtuala ;
}

void idatziMemoriaBirtualean32(uint32_t pageTableHelbidea, uint32_t helbideBirtuala, uint32_t balioa )
{
  uint32_t  helbideFisikoa = helbideFisikoaKalkulatu(pageTableHelbidea,helbideBirtuala) ;
  idatziMemoriaFisikoan32(helbideFisikoa,balioa);
}


uint32_t irakurriMemoriaBirtualetik32(uint32_t pageTableHelbidea, uint32_t helbideBirtuala)
{
uint32_t  helbideFisikoa = helbideFisikoaKalkulatu(pageTableHelbidea,helbideBirtuala) ;
  return irakurriMemoriaFisikotik32(helbideFisikoa);
}


uint32_t lortuEragiketa(uint32_t balioa)
{
        uint32_t emaitza;
        emaitza  = (balioa  & 0xf0000000) >> 28   ;
        return emaitza;
}
uint32_t lortuLehenErregistroa(uint32_t balioa)
{
        uint32_t emaitza;
        emaitza  = (balioa  & 0x0f000000) >> 24   ;
        return emaitza;
}
uint32_t lortuBigarrenErregistroa(uint32_t balioa)
{
        uint32_t emaitza;
        emaitza  = (balioa  & 0x00f00000) >> 20   ;
        return emaitza;
}
uint32_t lortuHirugarrenErregistroa(uint32_t balioa)
{
        uint32_t emaitza;
        emaitza  = (balioa  & 0x000f0000) >> 16   ;
        return emaitza;
}
uint32_t lortuHelbidea(uint32_t balioa)
{
        uint32_t emaitza;
        emaitza  = (balioa  & 0x00ffffff)    ;
        return emaitza;
}







/*
uint32_t loadEdoStore(uint8_t *src)
{
  if(irakurriEragiketa(&src[0]) == 0 || irakurriEragiketa(&src[0]) == 1){
    return irakurriMemoriatik24(&src[1]);
  }else{
    return irakurriBigarrenErregistroa(&src[0]);
  }

}
*/

void fitxategiaIrakurri(char* izena ){

char linea[1024];
  FILE *fich;

  fich = fopen(izena, "r");
  //Lee línea a línea y escribe en pantalla hasta el fin de fichero
  while(fgets(linea, 1024, (FILE*) fich)) {
      printf("Proba \n");
      printf("LINEA: %s \n", linea);
  }
  fclose(fich);
}

int lortuLerroKop (char* izena){
int kop=0;
char linea[1024];
  FILE *fich;

  fich = fopen(izena, "r");
    while(fgets(linea, 1024, (FILE*) fich)) {
      kop++;
    }
    fclose(fich);
    return kop-2;
  }

int lortuDataHelbidea (char* izena){
  char helbidea[7];
  char linea[1024];
  FILE *fich;

  fich = fopen(izena, "r");

  fgets(linea, 1024, (FILE*) fich);
  fgets(linea, 1024, (FILE*) fich);

  helbidea[0]= linea[6];

  helbidea[1]= linea[7];
  helbidea[2]= linea[8];
  helbidea[3]= linea[9];
  helbidea[4]= linea[10];
  helbidea[5]= linea[11];

  helbidea[6]= '\0';


  fclose(fich);

  return (int)strtol(helbidea, NULL, 16);

 }

 void kopiatuMemoriara(char* izena,int pageTableHelbidea ){
  int helbideBirtuala=0;
  int balioa = 0;
  char linea[1024];
  FILE *fich;

  fich = fopen(izena, "r");
  fgets(linea, 1024, (FILE*) fich);
  fgets(linea, 1024, (FILE*) fich);
  while(fgets(linea, 1024, (FILE*) fich)) {
    balioa= (int)strtol(linea, NULL, 16);
    idatziMemoriaBirtualean32(pageTableHelbidea,helbideBirtuala,balioa);
    helbideBirtuala = helbideBirtuala + 4;
  }
  fclose(fich);

 }
/*
void fitxategiaIrakurri(char* izena ){

//~ char ofilename[]= "salida.txt";
struct arrChar aux;int i=0,j=0,res;

FILE *ifp;
ifp=fopen(izena,"r");
    while (fscanf(ifp,"%s",aux.string) != EOF){
        j++;
    }
    printf("el archivo tiene %d valores",j);
fclose (ifp);

}
*/
/*
void karpetagoGuztiaIrakurri (char* karpeta){
  DIR *karpeta;

  while ((ent = readdir (karpeta)) != NULL)
   {
     // Nos devolverá el directorio actual (.) y el anterior (..), como hace ls
     if ( (strcmp(ent->d_name, ".")!=0) && (strcmp(ent->d_name, "..")!=0) )
   {
     // Una vez tenemos el archivo, lo pasamos a una función para procesarlo.
     procesoArchivo(ent->d_name);
   }
   }
 closedir (karpeta);

 return EXIT_SUCCESS;
}
}
*/

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
///////////////////////////////////////////process_loader///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

/*void *process_loader(void *parametroak){

  struct process_loader_parametroak_struct *param = (struct process_loader_parametroak_struct *)parametroak;
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
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////memoria_funtzioak///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
//hutsa dagon memoria ziti bat aurkitu eta erreserbatzen du. tamaina byte-tan. sistemarako = 1 baldin bada memoria erreserbatuan aurkitzen du bestela beste zatian
int lekuaDago(int pos, int tamaina ){

  for(int i = pos;i < (pos+tamaina);i+=4) {
    if(irakurriMemoriaEgoeratik32(i) != 0){
      return 0;
    }
  }
  return 1;

}


int erreserbatu_memoria(int tamaina){
  //Aurkitu pagetablen helbide huts bat, 4 byteekoa
  int pagetableHelbidea= -1;
  int libreMemoriaHelbidea = -1;
  int i;
  for(i = 0;i < (MEMORIA_ERRESERBATU_TAMAINA-4);i+=4)
  {
    if(irakurriMemoriaEgoeratik32(i)==0){
      pagetableHelbidea=i;
      break;
    }
  }

//Aurkitu libre dagoen memoria zatia
for(i = MEMORIA_ERRESERBATU_TAMAINA;i < MEMORIA_TAMAINA-tamaina;i+=4)
{
  if(lekuaDago(i, tamaina)){
    libreMemoriaHelbidea=i;
    break;
  }
}

//Biak aurkitu badira, beteta jarri
if(pagetableHelbidea!=-1 && libreMemoriaHelbidea!=-1){

  //Beteta jarri pagetableko helbidea
  idatziMemoriaEgoeran32(pagetableHelbidea,4);
  //Beteta jarri memoriako zatia
  idatziMemoriaEgoeran32(i,tamaina);
  for(i = libreMemoriaHelbidea+4; i < libreMemoriaHelbidea+tamaina;i+=4)
  {
    idatziMemoriaEgoeran32(i,-1);
  }

  //Bete pagetable
  idatziMemoriaFisikoan32(pagetableHelbidea,libreMemoriaHelbidea);


}
else{
  printf("Ez da lekurik aurkitu. (%d, %d)\n",pagetableHelbidea, libreMemoriaHelbidea );
}

  return pagetableHelbidea;

}

int askatu_memoria(int pageTableHelbidea){
  int helb;
  int helbideFisikoa= irakurriMemoriaFisikotik32(pageTableHelbidea);
  int tamaina= irakurriMemoriaEgoeratik32(helbideFisikoa);
  for (int i=0; i<tamaina; i+=4)
  {
     helb= helbideFisikoaKalkulatu(pageTableHelbidea,i);
     idatziMemoriaEgoeran32(helb,0);
     idatziMemoriaFisikoan32(helb,0);
  }
  idatziMemoriaEgoeran32(pageTableHelbidea,0);
  idatziMemoriaFisikoan32(pageTableHelbidea,0);

}

void inprimatuMemoriakoBetetakoak(){
  int i;

  for(i=0; i< MEMORIA_TAMAINA;i+=4)
  {
    if( ((int) irakurriMemoriaEgoeratik32(i))> 0){
      printf("Betetako helbidea: %d --> %d\n", i,  (int) irakurriMemoriaEgoeratik32(i) );
    }
  }


}


void inprimatuMemoriaEgoera( int erakutsiDatuak){
  int i,j;
  int pageTableHelbidea =-1;
  int helbideFisikoa =-1;
  int tamaina = -1;
  for(i=0; i< MEMORIA_ERRESERBATU_TAMAINA;i+=4)
  {
    if( irakurriMemoriaEgoeratik32(i)!= 0){
      pageTableHelbidea=i;
      helbideFisikoa= irakurriMemoriaFisikotik32(pageTableHelbidea);
      tamaina= irakurriMemoriaEgoeratik32(helbideFisikoa);

      printf("Betetako helbidea: %d --> %d (tam: %d)\n", pageTableHelbidea,  helbideFisikoa, tamaina );

      if(erakutsiDatuak==1){
        printf("Datuak:\n");
        for (j=0; j<tamaina; j+=4)
        {
          printf("\tDatua[%d] = %d \n", helbideFisikoaKalkulatu(pageTableHelbidea,j), irakurriMemoriaBirtualetik32(pageTableHelbidea,j));
        }
      }

    }
  }


}

//Imprimatu memoria datuak

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////process_loader///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

void *process_loader(void *parametroak){

  struct process_loader_parametroak_struct *param = (struct process_loader_parametroak_struct *)parametroak;
  char* fitxategiIzena="/home/aitor/unibersidadea/3maila/SE_Proiektuak/3_Faseak_SE/3.2Probak/Programak-20201213/prog000.elf";
  while(1){
    printf("%d\n",lortuLerroKop(fitxategiIzena) );
    printf("%d\n", lortuDataHelbidea(fitxategiIzena));
      //fitxategiaIrakurri("/home/aitor/unibersidadea/3maila/SE_Proiektuak/3_Faseak_SE/3.2Probak/Programak-20201213/prog000.elf");
      usleep(param->process_maiztasuna*cpu.erloju_maiztasuna);
      struct pcb_struct pcb;
      pcb.pid++;
      pcb.lehentasuna=rand() % 100;
      pcb.denbora=(rand() % 10000) ; //erloju ziklotan neurtua

      pcb.mm.pgb=erreserbatu_memoria(lortuLerroKop(fitxategiIzena)*4);
      pcb.mm.data=lortuDataHelbidea(fitxategiIzena);
      pcb.mm.code=0;
      kopiatuMemoriara(fitxategiIzena,pcb.mm.pgb);

printf("pgbe: %d\n",   pcb.mm.pgb);


      pthread_mutex_lock(&pcb_ilara->mutex);
      enqueue(pcb_ilara, pcb);
      pthread_mutex_unlock(&pcb_ilara->mutex);
      # ifdef DEBUG
      printf("procesua sortu dut.pid=  %d  procesu denbora %d \n", pcb.pid, pcb.denbora);
      printf("prozesu kopurua: %d \n", pcb_ilara->size);
      # endif
      inprimatuMemoriaEgoera(1);
      break;

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

  ///////////////////////////////////////////memoria hasieraketa///////////////////////////////////////////////////////

  memoria_fisikoa = malloc(MEMORIA_TAMAINA * sizeof(uint8_t));

  for(i = 0;i < MEMORIA_TAMAINA;i++) {
  memoria_fisikoa[i]=0;
  }

  memoria_fisikoa_egoera = malloc(MEMORIA_TAMAINA * sizeof(uint8_t));

  for(i = 0;i < MEMORIA_TAMAINA;i++) {
  memoria_fisikoa_egoera[i]=0;
  }



int number = (int)strtol("28670000", NULL, 16);

int helb = erreserbatu_memoria(16);
erreserbatu_memoria(4);

inprimatuMemoriaEgoera(1);

printf("\n\n#####################ASKATU ONDOREN\n" );


askatu_memoria(helb);
erreserbatu_memoria(4);
erreserbatu_memoria(4);
inprimatuMemoriaEgoera(1);









printf("\n\n#####################Proba\n" );


//inprimatuMemoriakoBetetakoak();
//idatziMemoriaBirtualean32(helb,0,17);
//memoria_fisikoa[0]=255;
/*idatziMemoriaFisikoan32(number,&memoria_fisikoa[0]);
uint32_t proba =irakurriMemoriaFisikotik32(&memoria_fisikoa[0]);


printf("memoria fisikoa npack0: %d\n" , (int )lortuEragiketa(proba) );
printf("memoria fisikoa npack0: %d\n" , (int )lortuLehenErregistroa(proba) );
printf("memoria fisikoa npack0: %d\n" , (int )lortuBigarrenErregistroa(proba) );
printf("memoria fisikoa npack0: %d\n" , (int )lortuHirugarrenErregistroa(proba) );
printf("memoria fisikoa npack0: %d\n" , (int )lortuHelbidea(proba) );
/*
printf("memoria fisikoa npack0: %d\n" , (int )irakurriMemoriatik8_ezker(&memoria_fisikoa[0]) );
printf("memoria fisikoa npack0: %d\n" , (int )irakurriMemoriatik8_eskuin(&memoria_fisikoa[0]) );
printf("memoria fisikoa npack0: %d\n" , (int )irakurriMemoriatik8_ezker(&memoria_fisikoa[1]) );
printf("memoria fisikoa npack0: %d\n" , (int )irakurriMemoriatik8_eskuin(&memoria_fisikoa[1]) );
*/
//printf("memoria fisikoa npack0: %d\n" , (int )loadEdoStore(&memoria_fisikoa[0]) );

//idatziMemorian32(-1000,&memoria_fisikoa[0]);
//printf("memoria fisikoa npack0: %d\n" , (int )irakurriMemoriatik32(&memoria_fisikoa[0]) );


  ///////////////////////////////////////////memoria bukaera///////////////////////////////////////////////////////

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
  struct process_loader_parametroak_struct process_parametroak;


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
  pthread_create(&processHaria, NULL, process_loader, (void *)&process_parametroak);

  /////////////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////CPU_Hariak///////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////////


  struct core_parametroak_struct *parametroak;

  cpu.coreHariak = malloc(cpu.core_kop * sizeof(struct coreHariak_struct));
  parametroak = malloc(cpu.core_kop * sizeof(struct core_parametroak_struct));

  for(i = 0; i < cpu.core_kop; i++){
      parametroak[i].core_id = i;
      err = pthread_create(&cpu.coreHariak[i].pthreada, NULL, core_funtzioa, (void *)&parametroak[i]);;

      if(err > 0){
          fprintf(stderr, "Errore bat gertatu da core haria sortzean.\n");
          exit(1);
      }
  }

  for(i = 0;i < cpu.core_kop;i++) {
    pthread_join(cpu.coreHariak[i].pthreada, NULL);
  }


  free(cpu.coreHariak);
  free(parametroak);






pthread_join(erlojuHaria, NULL);
pthread_join(timerHaria, NULL);
pthread_join(processHaria, NULL);


return 0;


}
