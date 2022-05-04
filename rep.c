#asm
    .equ __lcd_port=0x02
    .equ __lcd_EN=1
    .equ __lcd_RS=0
    .equ __lcd_D4=2
    .equ __lcd_D5=3
    .equ __lcd_D6=4
    .equ __lcd_D7=5
#endasm

#include <io.h>
#include <delay.h>
#include <stdio.h>
#include <ff.h>
#include <display.h>

//Código base que reproduce A001.WAV que es un WAV, Mono, 8-bit, y frec de muestreo de 22050HZ
    
char bufferL[256];
char bufferH[256]; 
char NombreArchivo[]  = "0:A001.wav";
unsigned int i=0;
unsigned int j=0;
unsigned int z=0;
bit LeerBufferH,LeerBufferL;
unsigned long muestras;
bit play;
char song=1;
bit stereo;


interrupt [TIM1_COMPA] void timer1_compa_isr(void)
{
disk_timerproc();
/* MMC/SD/SD HC card access low level timing function */
}
        
//Interrupción que se ejecuta cada T=1/Fmuestreo_Wav
interrupt [TIM2_COMPA] void timer2_compa_isr(void)         
{
  if (stereo==0)
  {
    if (i<256)   
    {
      OCR0A=bufferL[i];
      OCR0B=bufferL[i++];
    }  
    else      
    {
      OCR0A=bufferH[i-256]; 
      OCR0B=bufferH[i-256]; 
      i++;
    }   
    if (i==256)
       LeerBufferL=1;
    if (i==512)
    {
       LeerBufferH=1;
       i=0;
    }
  }
  else
  {
    if (i<256)
    {
      OCR0A=bufferL[i++];
      OCR0B=bufferL[i++]; 
    }  
    else      
    {
      OCR0A=bufferH[i-256];
      i++;
      OCR0B=bufferH[i-256];
      i++;
    }   
    if (i==256)
       LeerBufferL=1;
    if (i==512)
    {
       LeerBufferH=1;
       i=0;
    }
  }     
}

void main()
{
    unsigned int  br;
       
    /* FAT function result */
    FRESULT res;
                  
   
    /* will hold the information for logical drive 0: */
    FATFS drive;
    FIL archivo; // file objects 
                                  
    CLKPR=0x80;         
    CLKPR=0x01;         //Cambiar a 8MHz la frecuencia de operación del micro 
       
    // Código para hacer una interrupción periódica cada 10ms
    // Timer/Counter 1 initialization
    // Clock source: System Clock
    // Clock value: 1000.000 kHz
    // Mode: CTC top=OCR1A
    // Compare A Match Interrupt: On
    TCCR1B=0x0A;     //CK/8 10ms con oscilador de 8MHz
    OCR1AH=0x27;
    OCR1AL=0x10;
    TIMSK1=0x02; 
                                                    
    //PWM para conversión Digital Analógica WAV->Sonido
    // Timer/Counter 0 initialization
    // Clock source: System Clock
    // Clock value: 8000.000 kHz
    // Mode: Fast PWM top=0xFF
    // OC0A output: Non-Inverted PWM
    // TCCR0A=0x83;
    TCCR0A=0xA3;              
    
    DDRB.7=1;  //Salida bocina (OC0A)
    DDRG.5=1;  //Salida bocina (OC0B)
                                      
    // Timer/Counter 2 initialization
    // Clock source: System Clock
    // Clock value: 1000.000 kHz
    // Mode: CTC top=OCR2A
    ASSR=0x00;
    TCCR2A=0x02;
    TCCR2B=0x02;
    OCR2A=0x2C;
        
    // Timer/Counter 2 Interrupt(s) initialization
    TIMSK2=0x02;
    
    DDRD.7=1;
    #asm("sei")   
    disk_initialize(0);  /* Inicia el puerto SPI para la SD */
    delay_ms(500);  
    
    SetupLCD();
    PORTC.7=1;
    PORTC.6=1;
    PORTC.5=1;
    
    /* mount logical drive 0: */
    if ((res=f_mount(0,&drive))==FR_OK){  ;
        while(1)
        { 
        NombreArchivo[5]=song+'0';   
        /*Lectura de Archivo*/
        res = f_open(&archivo, NombreArchivo, FA_OPEN_EXISTING | FA_READ);
        if (res==FR_OK){ 
            PORTD.7=1;
            f_read(&archivo, bufferL, 44,&br); //leer encabezado            
            
            muestras=(long)bufferL[43]*16773216+(long)bufferL[42]*65536+(long)bufferL[41]*256+bufferL[40];
            f_lseek(&archivo,muestras+44+20);
            f_read(&archivo, bufferH, 100, &br);
            EraseLCD();
            for (j=0;j<100;j++){
                if(bufferH[j]==0){
                    z=j;
                    break;
                }   
                CharLCD(bufferH[j]);
            }
            MoveCursor(0,1);
            for (j=z;j<100;j++){   
                if (bufferH[j]==0x54){  
                    z=j+5;
                    break;  
                }  
            } 
            
            for (j=z;j<100;j++){
                if(bufferH[j]==0)
                  break;    
                CharLCD(bufferH[j]);     
            }
            
            f_lseek(&archivo,44);    
            
            //leer la frecuencia de muestreo del encabezado 
            if(bufferL[24]==0x22)
              OCR2A=0x2C;
            if(bufferL[24]==0xC0)
              OCR2A=0x29;
            if(bufferL[24]==0x80)
              OCR2A=0x3E;

            
            if (bufferL[22]==1)
                stereo=0;
            else
                stereo=1;
            i=0;
            
            f_read(&archivo, bufferL, 256,&br); //leer los primeros 512 bytes del WAV
            f_read(&archivo, bufferH, 256,&br);    
            LeerBufferL=0;            //banderas 
            LeerBufferH=0;
            TCCR0B=0x01;    //Prende sonido
            do{   
                 while((LeerBufferH==0)&&(LeerBufferL==0));    // si cualquiera de los dos sea dirente de 0 se sale del while
                 if (LeerBufferL)
                 {                       
                     f_read(&archivo, bufferL, 256,&br); //leer encabezado
                     LeerBufferL=0;  
                 }
                 else
                 { 
                     f_read(&archivo, bufferH, 256,&br); //leer encabezado
                     LeerBufferH=0;
                    
                 }            
                 
                 if (PINC.7==0){   //play y pausa
                    play=~play;
                    if (play==1){
                      TCCR0B=0x00; 
                      while (PINC.7==0);
                    }
                    else{
                      TCCR0B=0x01;  
                      while (PINC.7==0);
                    }         
                 } 
                 
                 if (PINC.5==0){     //reinicia cancion 
                    TCCR0B=0x00;
                    delay_ms(30); //rebote al presionar
                    f_lseek(&archivo,44);
                    while (PINC.7==0);
                    delay_ms(10); //rebote al soltar
                    TCCR0B=0x01;    //Prende sonido
                 } 
                 
                 if (PINC.6==0){    // cambia de cancion
                     TCCR0B=0x00;   
                     break;
                 }
                 
                 
                          
            }while(br==256);
            TCCR0B=0x00;   //Apaga sonido
            f_close(&archivo); 
            song++;
            if (song==7)
                song=1;
            
        }              
        }
    }
    f_mount(0, 0); //Cerrar drive de SD
    while(1);
}
