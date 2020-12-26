//-----------------------------------------------------------------------------
// Salvador Mendoza (salmg.net), 2021
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Code to relay 14a technology data aka bt14a by Salvador Mendoza
//-----------------------------------------------------------------------------
#include "standalone.h"
#include "proxmark3_arm.h"
#include "appmain.h"
#include "fpgaloader.h"
#include "util.h"
#include "dbprint.h"
#include "ticks.h"
#include "string.h"
#include "BigBuf.h"
#include "iso14443a.h"
#include "protocols.h"
//#include "cmd.h"

#include "usart.h" // Bluetooth reading & writing

void ModInfo(void) {
    DbpString("  HF - Relaying ISO/14443A data over Bluetooth - (Salvador Mendoza)");
}
/* This standalone implements one reading mode.
*
* Reading ISO-14443A technology is not limited to payment cards. This example
* was designed to open new possibilities relaying ISO-14443A data over Bluetooth. 
*
* Be brave enough to share your knowledge & inspire others. Salvador Mendoza.
*/
void RunMod() {
    StandAloneMode();
    Dbprintf(_YELLOW_(">>")  "Relaying ISO/14443A data over Bluetooth a.k.a. bt14arelay Started<<");
    FpgaDownloadAndGo(FPGA_BITSTREAM_HF);

    // For received Bluetooth package
    uint8_t rpacket[MAX_FRAME_SIZE] = { 0x00 };
    uint16_t lenpacket = 0;

    // For answering the commands
    uint8_t apdubuffer[MAX_FRAME_SIZE] = { 0x00 };
    uint8_t apdulen = 0;

    iso14a_card_select_t card_a_info;

    // For init ping process
    uint8_t sak = {0x0};
    uint8_t atqa[2] = { 0x00, 0x00 };
    uint8_t uidc[10] = { 0x00 };
    uint8_t uidlen = 0;
    uint8_t ats[MAX_FRAME_SIZE] = { 0x00 };
    uint8_t atsl = 0;

    uint8_t rdata[14] = { 0x00 }; 
    uint8_t scapec = 0x0A; //'\n'


#define STATE_READ 0

    uint8_t state = STATE_READ;
    for (;;) {
        WDT_HIT();
        int button_pressed = BUTTON_HELD(1000);

        if (button_pressed  == 1 || button_pressed == -1) //Pressing button to exit standalone mode
            break;

        if (state == STATE_READ) {
            LED_A_ON();
            clear_trace();
            set_tracing(true);

            iso14443a_setup(FPGA_HF_ISO14443A_READER_MOD);
            if (iso14443a_select_card(NULL, &card_a_info, NULL, true, 0, false)) {
                LED_B_ON();
                
                // Get data to send a ping with UID + ATQA + SAK + \n
                sak = card_a_info.sak;
                uidlen = card_a_info.uidlen;
                atsl = card_a_info.ats_len;

                memcpy(uidc, card_a_info.uid, uidlen);
                memcpy(atqa, card_a_info.atqa, 2);
                memcpy(ats,card_a_info.ats, atsl);

                DbpString(_YELLOW_("[ ") "UID:" _YELLOW_(" ]"));
                Dbhexdump(uidlen, uidc, false);
                DbpString(_YELLOW_("[ ") "ATQA:" _YELLOW_(" ]"));
                Dbhexdump(2, atqa, false);
                Dbprintf(_YELLOW_("[ ") "SAK: %x "_YELLOW_(" ]"), sak);
                DbpString(_YELLOW_("[ ") "ATS:" _YELLOW_(" ]"));
                Dbhexdump(atsl, ats, false);

                memcpy(&rdata[0], uidc, uidlen);
                memcpy(&rdata[uidlen], atqa, 2);
                memcpy(&rdata[uidlen+2], &sak, 1);
                memcpy(&rdata[uidlen+3], &scapec, 1);

                // ping = UID + ATQA + SAK + '\n'
                DbpString(_YELLOW_("[ ") "Ping:" _YELLOW_(" ]"));
                Dbhexdump(uidlen+3, rdata, false);

                DbpString(_YELLOW_("[ ") "Sending ping" _YELLOW_(" ]"));
                if (usart_writebuffer_sync(rdata,uidlen+4) == PM3_SUCCESS){
                    DbpString(_YELLOW_("[ ") "Sent!" _YELLOW_(" ]"));

                    for (;;) {
                        if (usart_rxdata_available()){
                            lenpacket = usart_read_ng(rpacket, sizeof(rpacket));

                            if (lenpacket > 1){
                                DbpString(_YELLOW_("[ ") "Bluetooth data:" _YELLOW_(" ]"));
                                Dbhexdump(lenpacket, rpacket, false);

                                apdulen = iso14_apdu(rpacket, (uint16_t) lenpacket, false, apdubuffer, NULL);
                                DbpString(_YELLOW_("[ ") "Card response:" _YELLOW_(" ]"));
                                Dbhexdump(apdulen - 2, apdubuffer, false);

                                apdubuffer[apdulen - 2] =  0x0A; //'\n';

                                usart_writebuffer_sync(apdubuffer, apdulen-1);
                            } else if(lenpacket == 1){
                                DbpString(_YELLOW_("[ ") "Done!" _YELLOW_(" ]"));
                                LED_C_ON();

                                for (uint8_t i = 0; i < 3; i++) 
                                    SpinDelay(1000);

                                break;
                            }
                        }
                        LED_B_OFF();
                    }
                } else{
                    DbpString(_YELLOW_("[ ") "Cannot send it!" _YELLOW_(" ]"));
                    SpinDelay(1000);
                }
            }
        }
    }
    DbpString(_YELLOW_("[=]") "exiting");
    LEDsoff();
}
