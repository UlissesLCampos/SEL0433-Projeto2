// SEL0433 - Entrega Final
// Feito por:
// Lucas Manoel Freitas da Silva - NºUSP: 15471884
// Gabriel Suerdieck Nardelli -    NºUSP: 15453960
// Ulisses Lombardi Campos -       NºUSP: 14781443
//
// Descrição:
// Aferidor de temperatura de forno industrial com PIC18F4550.

// === Pinos do LCD (padrão Kit EasyPIC v7)

sbit LCD_RS at RD2_bit;
sbit LCD_EN at RD3_bit;
sbit LCD_D4 at RD4_bit;
sbit LCD_D5 at RD5_bit;
sbit LCD_D6 at RD6_bit;
sbit LCD_D7 at RD7_bit;

sbit LCD_RS_Direction at TRISD2_bit;
sbit LCD_EN_Direction at TRISD3_bit;
sbit LCD_D4_Direction at TRISD4_bit;
sbit LCD_D5_Direction at TRISD5_bit;
sbit LCD_D6_Direction at TRISD6_bit;
sbit LCD_D7_Direction at TRISD7_bit;

// Pinos dos botões (PORTB)

sbit BOTAO_LONGA   at RB0_bit;   // INT0
sbit BOTAO_CURTA   at RB1_bit;   // INT1
sbit BOTAO_INICIAR at RB2_bit;   // INT2

// LED de resistência (RC0)
sbit LED_RESIST at RC0_bit;


// Variáveis globais de Controle

int   tempo_restante = 0;
short contando = 0;
short modo_selecionado = 0; // 0 = Nenhum, 1 = Curto, 2 = Longo
int   contador_auxiliar = 0;

// Rotina de interrupção

void interrupt() {

    // INT0 (RB0): seleciona contagem LONGA (60 s)
    if (INTCON.INT0IF == 1) {
        if (!contando) {
            modo_selecionado = 2;
            tempo_restante = 60;
        }
        INTCON.INT0IF = 0;
    }

    // INT1 (RB1): seleciona contagem CURTA (10 s)
    if (INTCON3.INT1IF == 1) {
        if (!contando) {
            modo_selecionado = 1;
            tempo_restante = 10;
        }
        INTCON3.INT1IF = 0;
    }

    // INT2 (RB2): INICIA a contagem e medição
    if (INTCON3.INT2IF == 1) {
        if (modo_selecionado > 0 && !contando) {
            contando = 1;

            if (modo_selecionado == 1) {
                // Inicia Timer 1 (Curta - 250ms)
                TMR1H = 0x0B; TMR1L = 0xDC;
                contador_auxiliar = 0;
                PIR1.TMR1IF = 0;
                PIE1.TMR1IE = 1;
                T1CON.TMR1ON = 1;
            } else {
                // Inicia Timer 0 (Longa - 1s)
                TMR0H = 0xE1; TMR0L = 0x4C;
                INTCON.TMR0IF = 0;
                INTCON.TMR0IE = 1;
                T0CON.TMR0ON = 1;
            }
        }
        INTCON3.INT2IF = 0;
    }

    // TMR0: base de ~1 s para contagem LONGA
    if (INTCON.TMR0IF == 1) {
        TMR0H = 0xE1; TMR0L = 0x4C;
        if (tempo_restante > 0) tempo_restante--;
        INTCON.TMR0IF = 0;
    }

    // TMR1: base de ~250 ms para contagem CURTA
    if (PIR1.TMR1IF == 1) {
        TMR1H = 0x0B; TMR1L = 0xDC;
        contador_auxiliar++;
        if (contador_auxiliar >= 4) {
            contador_auxiliar = 0;
            if (tempo_restante > 0) tempo_restante--;
        }
        PIR1.TMR1IF = 0;
    }
}


// Função principal

void main() {
    unsigned int  leitura_adc;
    unsigned long temp_x10;
    char linha_lcd[17]; // Buffer de 16 caracteres + terminador nulo

    // Configuração dos pinos
    CMCON  = 0x07;
    TRISA  = 0xFF; // Entradas Analógicas
    TRISB  = 0xFF; // Botões
    TRISC0_bit = 0; // Saída LED
    LED_RESIST = 0;

    // LCD
    Lcd_Init();
    Lcd_Cmd(_LCD_CLEAR);
    Lcd_Cmd(_LCD_CURSOR_OFF);

    // ADC
    ADC_Init();
    ADCON1 = 0x3B; // Referência Externa 1V

    // Configuração dos Timers
    T0CON = 0x07; // Prescaler 256
    T1CON = 0xB0; // Prescaler 8, 16-bit

    // Interrupções
    RCON.IPEN    = 0;
    INTCON.GIE   = 1;
    INTCON.PEIE  = 1;

    INTCON2.INTEDG0 = 1; INTCON.INT0IE  = 1; // Borda de subida RB0
    INTCON2.INTEDG1 = 1; INTCON3.INT1IE = 1; // Borda de subida RB1
    INTCON2.INTEDG2 = 1; INTCON3.INT2IE = 1; // Borda de subida RB2

    while (1) {
        if (contando) {
            // Leitura da Temperatura
            leitura_adc = ADC_Get_Sample(0);
            temp_x10 = ((unsigned long)leitura_adc * 1000UL) / 1023UL;

            // Controle da Resistência (Forno)
            // Acende LED para temp < 60°C (ADC < 614), apaga para temp > 80°C (ADC > 818)
            if (leitura_adc < 614)       LED_RESIST = 1;
            else if (leitura_adc > 818)  LED_RESIST = 0;

            // --- Atualização da Linha 1 (Temperatura)
            linha_lcd[0] = 'T'; linha_lcd[1] = 'e'; linha_lcd[2] = 'm'; linha_lcd[3] = 'p'; linha_lcd[4] = ':'; linha_lcd[5] = ' ';
            linha_lcd[6] = (temp_x10 / 100) + '0';           // Dezena
            linha_lcd[7] = ((temp_x10 / 10) % 10) + '0';     // Unidade
            linha_lcd[8] = '.';                              // Ponto
            linha_lcd[9] = (temp_x10 % 10) + '0';            // Decimal
            linha_lcd[10] = ' '; linha_lcd[11] = 223; linha_lcd[12] = 'C';
            linha_lcd[13] = ' '; linha_lcd[14] = ' '; linha_lcd[15] = ' '; linha_lcd[16] = '\0';
            Lcd_Out(1, 1, linha_lcd);

            // Atualização da Linha 2 (Tempo)
            linha_lcd[0] = 'T'; linha_lcd[1] = 'e'; linha_lcd[2] = 'm'; linha_lcd[3] = 'p'; linha_lcd[4] = 'o'; linha_lcd[5] = ':'; linha_lcd[6] = ' ';
            linha_lcd[7] = (tempo_restante / 10) + '0';
            linha_lcd[8] = (tempo_restante % 10) + '0';
            linha_lcd[9] = 's';
            linha_lcd[10] = ' '; linha_lcd[11] = ' '; linha_lcd[12] = ' '; linha_lcd[13] = ' '; linha_lcd[14] = ' '; linha_lcd[15] = ' '; linha_lcd[16] = '\0';
            Lcd_Out(2, 1, linha_lcd);

            // Fim da Contagem
            if (tempo_restante <= 0) {
                contando = 0;
                modo_selecionado = 0;
                LED_RESIST = 0;

                // Desliga os Timers
                T0CON.TMR0ON = 0; T1CON.TMR1ON = 0;
                INTCON.TMR0IE = 0; PIE1.TMR1IE = 0;

                Lcd_Cmd(_LCD_CLEAR);
                Delay_ms(10);
                Lcd_Out(1, 1, "Fim da medicao!");
                Delay_ms(2000);
                Lcd_Cmd(_LCD_CLEAR);
                Delay_ms(10);
            }

        } else {
            //  Modo de Seleção
            Lcd_Out(1, 1, "Sel. modo:      ");
            if (modo_selecionado == 1)      Lcd_Out(2, 1, "Curta: 10s      ");
            else if (modo_selecionado == 2) Lcd_Out(2, 1, "Longa: 60s      ");
            else                            Lcd_Out(2, 1, "-- / --         ");
        }

        Delay_ms(150); // Delay suave para estabilizar a simulação
    }
}