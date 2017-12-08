# Data Logger de temperatura


## Resumo

Projeto de um sistema embarcado completo de um data logger de temperatura.


## Características do projeto

Desenvolvido com:

- **PIC PIC18F4550**
- **Linguagem C**
- **Proteus**


## Composição do projeto

Projeto dividido em:

- **Esquema elétrico:** Imagem do esquema elétrico de montagem do projeto.
- **Projeto:** Pasta com o código fonte do projeto em C.
- **Simulacao no Proteus:** Pasta com os arquivos para simulação no Proteus.
- **hyperterminal:** Pasta com o executável do hyperterminal para testes reais.


## Descrição detalhada

- Leitura do sensor LM35 através do conversor A/D do PIC18F4550. para obter dados de temperatura.

- Leitura do circuito integrado HT1380 através da comunicação "SPI" do PIC18F4550 para obter o horário (hora,minuto,segundo) e a data (dia, mês e ano) referente a temperatura.

- Armazenamento dos dados (temperatura, horário, data) na memória E2PROM do circuito integrado 24C08 através da comunicação I2C do PIC18F4550.

- Escrita no display LCD (16x2) para informar ao usuário a data e a hora atual , assim como o valor da última temperatura lida.

- Conexão com o computador (PC) através da porta serial do PIC18F4550 para comunicação com o datalogger.
	* 2400 bps.
	* 8 bits.
	* Sem paridade
	* Stop Bit.

- Leitura do botão PUSH-BUTTON para que o datalogger de temperatura entre nos seguintes modos de operação: CONFIG e RUN.

- Leitura da chave SWITCH para que o usuário interrompa a coleta de dados do datalogger (se no modo RUN) ou para que o usuário entre no modo de configuração do datalogger via Hyperterminal do Windows (se no modo CONFIG).

- Acionamento do LED para indicar ao usuário que o data logger encontra-se no modo RUN.

- MODO RUN: chave CH3 em nível lógico alto. Neste modo, ao ligar o microcontrolador, o datalogger faz a coleta dos dados a cada 1 minuto e armazena os dados na EEPROM. Para encerrar a coleta de dados, o usuário deve pressionar o botão SW14. Para descarregar os dados da EEPROM pela porta serial, o usuário deve optar pelo MODO CONFIG.

- MODO CONFIG: chave CH3 em nível lógico baixo. Ao ligar o microcontrolador, o usuário deve pressionar a chave SW14. Deverá aparecer um menu com as seguintes
opões:
	1. **Configuração:** ao pressionar a tecla 1, o usuário entra com a data, a hora e a base de tempo do datalogger, como mostrado abaixo:
		29/11/08 14:28:00 1
	2. **EEPROM:** ao presionar a tecla 2, os dados coletados são lidos diretamente da EEPROM e enviados ao PC pelo Hyperterminal, como mostrado abaixo:
		29/11/08 14:29:01 22,5
		29/11/08 14:30:01 23,5
	3. **Sair:** sai do MODO CONFIG.