#include <p18f4550.h>
#include <usart.h>
#include <delays.h>
#include<adc.h>
#include<usart.h>
#include<Timers.h>
#include<i2c.h>
#include "xlcd_mod.c"
//OBS: Para alterar a pinagem do LCD edite o arquivo xlcd.h

#pragma config	PLLDIV = 5		// PLL para 20MHz
#pragma config FOSC = HS  		// Fosc = 20MHz Tosc=1/20MHz
#pragma config CPUDIV = OSC1_PLL2  // PLL desligado
#pragma config PBADEN = OFF  	// Pinos do PORTB come�am como digitais
#pragma config WDT = OFF  	// Watchdog desativado
#pragma config LVP = OFF  	// Desabilita grava��o em baixa tens�o
#pragma config DEBUG = ON  	// habilita debug

#define LED PORTAbits.RA5	//Defina a porta do LED_2G

#define PIO		PORTBbits.RB0	// Pino IO do RTC
#define PSCLK	PORTBbits.RB1	// Pino SCLK do RTC
#define PREST	PORTAbits.RA4	// Pino REST do RTC


// Declara��o de vari�veis globais
unsigned char year,month,day,hour,minute,second;	//Vari�veis para as fun��es de data e hora
unsigned char ch;	//Recebe bit no MENU feito pela USART
int dia, mes, ano, hora, minuto, segundo;	//Vari�veis fixas para os valores de data e hora
int result;	//Recebe o resultado da conver��o AD
unsigned long short temp;	//Recebe o calculo da convers�o AD
char temperatura[] = {"000,0"};	//Recebe os caracters da conver��o AD
unsigned char alterar_data_hora[12];	//Vetor que recebe os dados para alterar a data e a hora
int i;	//Contador
unsigned char base_tempo;	//Base de tempo que multiplica o numero de minutos
int x = 0; //Controle da posi��o de escrita da memoria
int total = 0;	//Total de grava��es de data, hora e temperatura
unsigned char leitura[1] ; //Usado para a leitura da memoria
unsigned char res[]={"00"}; //Usados para convers�o
unsigned char temp3[] = {"0"};//Usado para convers�o
unsigned char tempAux[] = {"000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0"}; // Vetor auxiliar para enviar as temperaturas
unsigned int contTemp=0;	//Vari�vel de controle de posi��o


// Prot�tipos de Fun��es do RTC
void send_char(unsigned char sentchar);
unsigned char get_char();
void atraso(unsigned char time);
void set_time();
void set_wp_off();
void set_wp_on(); 
void read_time();
void set_ch_on();
void PrintBCD2(char data);

// Prot�tipos das demais fun��es
void ArmazenaMemoria(void);
void alterar_data_hora_base();
void TransmissaoEEPROM();
void leituraEEPROM(unsigned char banco, int x_leitura);
void converteArmazena(unsigned char banco, unsigned char valor);


// *** Prot�tipos
void LowPriorityISR(void);
void HighPriorityISR(void);

// *** Configura��o das RTI
// Interrup��o de baixa prioridade
#pragma code low_vector=0x18
void interrupt_at_low_vector(void)
{
	_asm GOTO LowPriorityISR _endasm
}
#pragma code

// Interrup��o de baixa prioridade
#pragma code high_vector=0x08
void interrupt_at_high_vector(void)
{
	_asm GOTO HighPriorityISR _endasm
}
#pragma code

// Defina no par�metro SAVE os registros a serem salvos antes da ISR
#pragma interruptlow LowPriorityISR
#pragma interrupt HighPriorityISR

// ISR de alta prioridade OU se n�o houver prioridade.
void HighPriorityISR(void)
{
	//CONFIG
	//Mostra o MENU
	putrsUSART("\r\n1-Configuracao \r\n2-EEPROM \r\n3-Sair\r\n");//envia mensagem serialmente
	while(!DataRdyUSART());
	if(PIR1bits.RCIF){
		ch = getcUSART();
		switch(ch){
			case '1':
			alterar_data_hora_base();
			while (BusyUSART()); //espera buffer txreg esvaziar
			break;
			case '2':
			putrsUSART("   EEPROM");
			while (BusyUSART()); //espera buffer txreg esvaziar
			//EEPROM
			TransmissaoEEPROM();
			break;
			case '3':
			putrsUSART("   Sair");
			while (BusyUSART()); //espera buffer txreg esvaziar
			//Sair
			break;
		}
	}
	INTCON3bits.INT2IF=0;  //zera flag da interrup��o INT2

}
//----------------------------------------------------------------------

int cont_tempo=0; //Contador de estouros do timer0
// ISR de baixa prioridade
void LowPriorityISR (void)
{
	cont_tempo++;
	if (cont_tempo == 60){ // Espera 60 segundos
		cont_tempo = 0;
		LED =~ LED; //Alterna a LED sinalizando grava��o

		//Armazena os dados na EEPROM
		ArmazenaMemoria();
		Delay10KTCYx(20);
		//Reconfigura os pinos do RCL
		TRISBbits.TRISB1=0; // Pino SCLK (RTC) sa�da
		TRISAbits.TRISA4=0; // Pino PREST (RTC) sa�da
		read_time(); //Passa os valores para as variaveis
	}
	TMR0H = 0X67; // Carrega valor inicial do timer
	TMR0L = 0x6A;
	INTCONbits.TMR0IF = 0; // zera flag da int. TMR0
}


void main (void){

	TRISBbits.TRISB3 = 1; //Define o PUSH como entrada
    TRISBbits.TRISB0 = 0; //Define o SDA como sa�da
	TRISBbits.TRISB1 = 0; //Define o SCL como sa�da
	TRISAbits.TRISA5 = 0; //Define a porta da LED como sa�da
	LED=1; // LED inicia ligada
//------------------------------Display--------------------------------------
	// Display LCD
	TRISD = 0X00;			//Declara as saidas do LCD
	TRISE = 0X00;

	// Inicializa��o do Display LCD
	OpenXLCD( FOUR_BIT & LINES_5X7 );
	WriteCmdXLCD(0x01);
	Delay10KTCYx(10);
//------------------------------Interrup��es----------------------------------
	//Configura��o do timer 0
	INTCONbits.TMR0IF = 0; // zera flag da interrup��o TMR0
	INTCONbits.TMR0IE = 1; // habilita interrup��o TMR0
	INTCON2bits.TMR0IP = 0; // interrup��o TMR0 � baixa

	TRISBbits.TRISB2 = 1; //Define o SWITCH como entrada
	INTCON2bits.RBPU=0; //resistores de pull-up da PORTB

	//Interrup��o INT2
	INTCON3bits.INT2IF=0;  //zera flag da interrup��o INT2
	INTCON3bits.INT2IE=1;  //habilita interrup��o INT2
	INTCON3bits.INT2IP=1;  //interrup��o INT2 � alta
	INTCON2bits.INTEDG2=0; // Borda de descida

	//Configura��o das interrup��es
	RCONbits.IPEN = 1; // habilita prioridade
	INTCONbits.GIEL = 1; // habilita int. baixa prioridade
	INTCONbits.GIEH = 1; // habilita int. alta prioridade
	/****************************************************************
	Configura��o do timer 0 -> T0CON (estouro a cada 1s)
	TMR0ON=1; T08BIT=0;->16 bits; T0CS=0??> clock interno
	T0SE=1->borda de descida do clock;
	PSA=0->prescaler ativado;
	T0PS2:T0PS0=110-> prescaler 128
	****************************************************************/
	T0CON = 0b10010110;

	// 0x676A = 26474
	// T=Tcy*prescaler*(65536 - 26474) = 0,9999872 segundos
	TMR0H = 0X67;
	TMR0L = 0x6A;

	//------------------------------USART----------------------------------
	TRISCbits.TRISC6 = 1; // pino TX entrada
	TRISCbits.TRISC7 = 1; // pino RX entrada
	//Configura��o da porta serial
	BAUDCONbits.BRG16 = 0; // gera��o do baud rate com 8 bits
	TXSTAbits.SYNC = 0; // modo ass�ncrono
	TXSTAbits.BRGH = 0; // baud rate de baixa taxa
	SPBRG = 129;
	RCSTAbits.SPEN = 1; //habilita pinos TX e RX para serial
	RCSTAbits.RX9 = 0; //recep��o sem o nono bit
	TXSTAbits.TXEN =1; // habilita a transmiss�o serial 
	RCSTAbits.CREN = 1; //modo recep��o cont�nua
	/****************************************************************
	Calculo do Baud Rate:
	Para 2400 bps com baud rate baixo e de 8 bits
	BRG = ((Fosc) /(br *64) - 1) ==> BRG =129,2 ==> BRG = 129
	Baud Rate calculado:
	br(calc) = (Fosc)/(64 *(BRG +1))  ==> br(calc) = 2403,84 bps
	erro% = ((|br(desejado) - br(calc)|)/(br(desejado))) ==> erro% = 0,16% de erro
	*****************************************************************/

	//------------------------------Real Time Clock-----------------------------
	TRISCbits.TRISC0=1; // Pino SW10 entrada
	TRISBbits.TRISB1=0; // Pino SCLK (RTC) sa�da
	TRISAbits.TRISA4=0; // Pino PREST (RTC) sa�da
	PREST=0;
	PSCLK=0;
	set_wp_off(); // Desabilita o write protect (WP=0)
	set_ch_on(); // Habilita o oscilador (CH=0)
	year=0x15; //Ano 2015
	month=0x05; //M�s 04
	day=0x19; //Dia 22
	hour=0x19; //Hora 12
	minute=0x00; //Minutos 00
	second=0x00; //Segundos 00
	set_time();  //Passa par�metros

	while(1){

		//------------------------Realiza covers�o A/D-------------------------------
		//configure A/D conversosr
		OpenADC (ADC_FOSC_32 &
        	ADC_RIGHT_JUST &
        	ADC_20_TAD,
        	ADC_CH0 &
        	ADC_INT_OFF &
        	ADC_REF_VDD_VSS,
        	ADC_1ANA);
		Delay10TCYx(10);    //Delay for 50TCY
		ConvertADC();       //Start conversion
		while(BusyADC());   //wait for completion
		result = ReadADC(); //Read result
		CloseADC();         //Disable A/D converter
		temp = ((long int) 500 * 10 * result) / 1023; //Recebe a convers�o em um temporario
		//Passa cada caractere para uma posicao do vetor
		temperatura[0] = 0x30 + (temp/1000); 
		temp %= 1000;
		temperatura[1] = 0x30 + (temp/100);
		temp %= 100;
		temperatura[2] = 0x30 + (temp/10);
		temperatura[3] = ('.');
		temperatura[4] = 0x30 + (temp%10);

		//Mostra no display a temperatura
		WriteCmdXLCD(0x80); // Posi��o cursor linha=1 coluna=1
		WriteCmdXLCD(0x0C); // Controle do display (ativo, sem cursor)
		putsXLCD(temperatura);
		WriteCmdXLCD(0x85); // Posi��o cursor linha=1 coluna=6
		putrsXLCD("oC");

		//Mostra no display a data e a hora
		CloseI2C(); //Finaliza a biblioteca I2C para usar o RTC
		TRISAbits.TRISA4 =0; //Reconfigura as portas do RTC
		TRISBbits.TRISB1 =0;
		read_time(); //Passa os valores de hora e data para as variaveis
		dia=day;
		mes=month;
		ano=year;
		hora=hour;
		minuto=minute;
		segundo=second;
		WriteCmdXLCD(0x88);	// Posi��o cursor linha=1 coluna=9		
		PrintBCD2(day);		//Escreve o DIA no LCD
		WriteCmdXLCD(0x8A); // Posi��o cursor linha=1 coluna=11
		putcXLCD('/');
		Delay10KTCYx(1);
		WriteCmdXLCD(0x8B); // Posi��o cursor linha=1 coluna=12
		PrintBCD2(month); 	//Escreve o MES no LCD
		WriteCmdXLCD(0x8D); // Posi��o cursor linha=1 coluna=14
		putcXLCD('/');
		Delay10KTCYx(1);
		WriteCmdXLCD(0x8E); // Posi��o cursor linha=1 coluna=15
		PrintBCD2(year);	//Escreve o ANO no LCD
		
		WriteCmdXLCD(0xC0); // Posi��o cursor linha=2 coluna=1
		PrintBCD2(hour);	//Escreve as HORAS no LCD
		putcXLCD(':');
		PrintBCD2(minute);	//Escreve os MINUTOS no LCD
		putcXLCD(':');
		PrintBCD2(second);	//Escreve os SEGUNDOS no LCD
  }
}


//Altera os valores de hora, data e a base de tempo do rel�gio
void alterar_data_hora_base(){
	putrsUSART("   Informe a data, a hora e a base de tempo:\r\n"); // -------------TESTE-----
	//Configura��o
	putrsUSART("DD,MM,AA,HH,MM,SS,B\r\n"); //Escreve o modelo de recep��o dos dados
	//Recebe os dados
	for(i=0; i<12; i++){
		while(!DataRdyUSART());
		alterar_data_hora[i]=getcUSART();
		WriteUSART(alterar_data_hora[i]);
		i++;
		while(!DataRdyUSART());
		alterar_data_hora[i]=getcUSART();
		WriteUSART(alterar_data_hora[i]);
		putrsUSART(",");
	}
	while(!DataRdyUSART()); 
	base_tempo=getcUSART();
	WriteUSART(base_tempo);
	putrsUSART("\r\n");

	//Converte os valores recebidos(char) para hexadecimal
	day= (alterar_data_hora[0]-0x30)<<4 | (alterar_data_hora[1]-0x30); //Altera Dia
	month= (alterar_data_hora[2]-0x30)<<4 | (alterar_data_hora[3]-0x30); //Altera M�s
	year= (alterar_data_hora[4]-0x30)<<4 | (alterar_data_hora[5]-0x30); //Altera Ano
	hour= (alterar_data_hora[6]-0x30)<<4 | (alterar_data_hora[7]-0x30); //Altera Hora
	minute= (alterar_data_hora[8]-0x30)<<4 | (alterar_data_hora[9]-0x30); //Altera Minutos
	second= (alterar_data_hora[10]-0x30)<<4 | (alterar_data_hora[11]-0x30); //Altera Segundos
	set_time();  //Passa par�metros
}

//Descarrega os valores armazenados na mem�ria EEPROM atrav�s da comunica��o USART 
void TransmissaoEEPROM(){
	int total_leitura = 1;	//Variavel de controle do n�mero de leituras realizadas
	int x_leitura=0; 	//Variavel de controle da posi��o na mem�ria
	unsigned char banco;	//Variavel que indica o banco utilizado (Banco 0 ou Banco 1)
	int j;	//Vaiavel de controle do envio da temeratura
	contTemp = 0;
	
	OpenI2C(MASTER, SLEW_ON); //Abre a biblioteca I2C usada para a mem�ria
	SSPADD = 11; //Velocidade da transmiss�o (clock) de 400 kHz
	Delay10KTCYx(1);
	while(total_leitura<=total){
		WriteCmdXLCD(0x80);

		if(total_leitura<15){
			banco = 0xA0; //Leitura at� 15 vezes no banco 0
		}else if(total_leitura>=15){
			banco = 0xA2; //Leitura at� 15 vezes no banco 1
		}

		putcUSART(' ');	//Espa�o
		while(BusyUSART());
		putrsUSART("\n\r");	//Pula linha
		while(BusyUSART());

		for(j = 0; j < 5; j++){//La�o respons�vel por enviar a temperatura para o computador
			WriteUSART(tempAux[j+contTemp]);//Enivia a temperatura, n�mero por n�mero
			while(BusyUSART());
		}
		putrsUSART("oC");
     	while(BusyUSART());
		contTemp+=5;

		putcUSART(' ');	//Espa�o
		while(BusyUSART());
		
		//------TEMPERATURA AD
		leituraEEPROM(banco, x_leitura);//Leitura do 1� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria
		leituraEEPROM(banco, x_leitura);//Leitura do 1� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria

		putcUSART(' ');	//Espa�o
		while(BusyUSART());

        //------DIA
		leituraEEPROM(banco, x_leitura);//Leitura do 1� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria
		leituraEEPROM(banco, x_leitura);//Leitura do 2� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria

		putcUSART('/');
		while(BusyUSART());

        //------MES
		leituraEEPROM(banco, x_leitura);//Leitura do 1� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria
		leituraEEPROM(banco, x_leitura);//Leitura do 2� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria

		putcUSART('/');
		while(BusyUSART());

        //------ANO
		leituraEEPROM(banco, x_leitura);//Leitura do 1� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria
		leituraEEPROM(banco, x_leitura);//Leitura do 2� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria

        //------ESPA�O
		putcUSART(' ');
		while(BusyUSART());

		//------HORA
		leituraEEPROM(banco, x_leitura);//Leitura do 1� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria
		leituraEEPROM(banco, x_leitura);//Leitura do 2� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria

		putcUSART(':');
		while(BusyUSART());

		//------MINUTO
		leituraEEPROM(banco, x_leitura);//Leitura do 1� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria
		leituraEEPROM(banco, x_leitura);//Leitura do 2� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria

		putcUSART(':');
		while(BusyUSART());

		//------SEGUNDO
		leituraEEPROM(banco, x_leitura);//Leitura do 1� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria
		leituraEEPROM(banco, x_leitura);//Leitura do 2� caractere
		x_leitura++;	//Avan�a uma posi��o na mem�ria

		//-----------------

		total_leitura++; //Incrementa controlador do n�mero de leituras realizadas
		if(total_leitura==15){	//Se terminou de ler o banco 0
			x_leitura=0; //Inicia controlador de mem�ria na primeira posi��o para utilizar o banco 1
		}

	}
	//Depois de descarregar pela USART as vari�veis s�o zeradas
	total=0;	//Zera vari�vel do total de grava��es
	x=0;	//Inicia controlador de mem�ria na primeira posi��o
	contTemp = 0;

	putrsUSART("\n\r");	//PULA LINHA
	CloseI2C();	//Fecha biblioteca IC2
	TRISBbits.TRISB0 = 0; //SDA
	TRISBbits.TRISB1 = 0; //SCL
}

//Realiza a leitura da mem�ria e descarrega pela USART
void leituraEEPROM(unsigned char banco, int x_leitura){
	EESequentialRead(banco,0x00 + x_leitura,temp3,2); //Faz a leitura na EEPROM
	Delay10KTCYx(1);
	WriteUSART(temp3[0]);	//Envia pela USART
	while(BusyUSART());
}

//Converte dois valores e armazena na mem�ria EEPROM
void converteArmazena(unsigned char banco, unsigned char valor){
	//Vari�veis para uxiliar a convers�o
	char in2,valor2,save2;
	char conv[]={"00"};
	
	//Convers�o feita com base nas fun��es do RTC	
	save2 = valor;
	valor2 = valor >>= 4;
	in2 = (valor2 & 0x0F);
	conv[0] = in2 + 0x30;
	in2 = (save2 & 0x0F);
	conv[1] = in2 + 0x30;	
	Delay10KTCYx(1);

	EEPageWrite(banco,0x00 + x,conv);	//Escreve na EEPROM
	Delay10KTCYx(1);
}

//Realiza o processo de armazenamento na mem�ria EEPROM
void ArmazenaMemoria(void){
	unsigned char banco;	//Variavel de controle do banco utilizado
	if(total<15){
		banco = 0xA0; //Armazena at� 15 vezes no banco 0
	}else{
		banco = 0xA2; //Armazena at� 15 vezes no banco 1
	}

	read_time();	//Passa os valores de hora e data para as vari�veis

	OpenI2C(MASTER, SLEW_ON); //Abre a biblioteca I2C usada para a mem�ria
	SSPADD = 11; //Velocidade da transmiss�o (clock) de 400 kHz
	Delay10TCYx(1);

	if(total<30){ //Armazena at� 30 valores no total
		total = total+1; //Incrementa a variavel de controle

		tempAux[0+contTemp] = temperatura[0];
		tempAux[1+contTemp] = temperatura[1];
		tempAux[2+contTemp] = temperatura[2];
		tempAux[4+contTemp] = temperatura[4];
		contTemp+=5;
	
		itoa(result,res);  //Converte o valor de ad para uma string
		Delay10KTCYx(1);
		EEPageWrite(banco,0x00 + x,res); //Salva na mem�ria o valor de ad
		Delay10KTCYx(1);
		x+=2;	//Avan�a duas posi��es na mem�ria

		converteArmazena(banco,dia);	//Grava o DIA
		x+=2;	//Avan�a duas posi��es na mem�ria
		Delay10KTCYx(1);
		
		converteArmazena(banco,mes);	//Grava o MES
		x+=2;	//Avan�a duas posi��es na mem�ria
		Delay10KTCYx(1);
		
		converteArmazena(banco,ano);	//Grava o ANO
		x+=2;	//Avan�a duas posi��es na mem�ria
		Delay10KTCYx(1);
		
		converteArmazena(banco,hora);	//Grava as HORAS
		x+=2;	//Avan�a duas posi��es na mem�ria
		Delay10KTCYx(1);
		
		converteArmazena(banco,minuto);	//Grava os MINUTOS
		x+=2;	//Avan�a duas posi��es na mem�ria
		Delay10KTCYx(1);
		
		converteArmazena(banco,segundo);	//Grava os SEGUNDOS
		x+=2;	//Avan�a duas posi��es na mem�ria

		if(total==15){	//Se terminou de escrever no banco 0
			x=0; //Inicia no banco 1
		}

	}
	Delay10KTCYx(1);
	CloseI2C(); //Finaliza a biblioteca I2C para usar o RTC
	//Reconfigura as portas do RTC
	TRISBbits.TRISB0 = 0; //SDA
	TRISBbits.TRISB1 = 0; //SCL

}

//------------------------------------------------FUN��ES DO RTC (REAL TIME CLOCK)-------------------------------------------------------------------

// Fun��o envia caracter para o ht1380
void sent_char(unsigned char sentchar)
{
	unsigned char n;
	TRISBbits.TRISB0=0; 
	for(n=0;n<8;n++)
	{
		PSCLK=0;
		if ((sentchar&1)!=0)
		{ 
			PIO=1;
		}
		else
		{ 
			PIO=0;
		}
		sentchar=sentchar>>1;
		atraso(2);
		PSCLK=1;
		atraso(2);		
	}
}
	
// Fun��o recebe caracter do ht1380
unsigned char get_char()
{
	unsigned char n;
	unsigned char getchar=0;
	unsigned char temp=1;
	TRISBbits.TRISB0=1;
	for(n=0;n<8;n++)
	{
		PSCLK=0;
		atraso(1);
		if(PORTBbits.RB0==1)
		{ 
			getchar=getchar|temp;
		}
		temp=temp<<1;
		atraso(2);
		PSCLK=1;
		atraso(2);
	}
	return (getchar);
}

void read_time()   
 {   
  unsigned char temp;   
    PREST=1; 
	PSCLK=0;  
    sent_char(0xbf);      //10111111b   
    second=get_char();   
    minute=get_char();   
    hour=  get_char();   
    day=   get_char();   
    month= get_char();   
    temp=  get_char();   /* week day */   
    year=  get_char();   
    temp=  get_char();               
    PSCLK=0;   
    atraso(2);   
    PREST=0;   
    atraso(10);    
 }    

// Fun��o para gerar atraso 
void atraso(unsigned char time)
{ 
  unsigned int i=0; 
  for(i=0;i<=time;i++){} 
} 

// Fun��o set_time
void set_time()   
 {   
   set_wp_off();   
   PREST=1;   
   atraso(4);   
   second=second & 0x7f;   
   sent_char(0xbe);      //10111110b   
   sent_char(second);   
   sent_char(minute);   
   sent_char(hour);   
   sent_char(day);   
   sent_char(month);   
   sent_char(1);         /* week  day */   
   sent_char(year);   
   sent_char(0);   
   PSCLK=0;   
   atraso(4);    
   PREST=0;  
   atraso(4);   
   set_wp_on();   
 } 

// Desabilita o write protect (WP=0)
void set_wp_off()   
{   
    PREST=1;   
    atraso(4);    
    sent_char(0x8e);     //10001110b   
    sent_char(0);        //00000000b   
    PSCLK=0;   
    atraso(4);    
    PREST=0;   
    atraso(4);
} 

// Habilita o write protect (WP)
void set_wp_on()   
{   
  PREST=1;   
  atraso(4);  
  sent_char(0x8e);       //10001110b   
  sent_char(0x80);       //10000000b   
  PSCLK=0;   
  atraso(4);   
  PREST=0;   
  atraso(4);  
} 

// Habilita o oscilador (CH=0)
void set_ch_on()   
{   
  unsigned int i; 
  PREST=1;   
  atraso(4);  
  sent_char(0x80);       //10001110b   
  sent_char(0);       	//00000000b   
  PSCLK=0;   
  atraso(4);   
  PREST=0;   
  atraso(4); 
  // atraso de 3s para estabilizar o oscilador do RTC
  for(i=0;i<=5;i++)
  {
	Delay10KTCYx(250); // Atraso de 0.5s
  }  
} 

void PrintBCD2 (char data)
{
	char in,valor,save;		
	save = data;
	valor = data >>= 4;
	in = (valor & 0x0F);
	putcXLCD (in + 0x30);
	in = (save & 0x0F);
	putcXLCD (in + 0x30);
}