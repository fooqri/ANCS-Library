#include <Arduino.h>
#include "aci_bonding.h"


/*
   Read the Dymamic data from the EEPROM and send then as ACI Write Dynamic Data to the nRF8001
   This will restore the nRF8001 to the situation when the Dynamic Data was Read out
 */
aci_status_code_t bond_data_restore(aci_state_t *aci_stat, hal_aci_data_t* aci_cmd, hal_aci_evt_t* aci_data, uint8_t eeprom_status, bool *bonded_first_time_state) {
    aci_evt_t *aci_evt; 
    uint8_t eeprom_offset_read = 1;
    uint8_t write_dyn_num_msgs = 0;
    uint8_t len =0;

    // Get the number of messages to write for the eeprom_status
    write_dyn_num_msgs = eeprom_status & 0x7F;

    //Read from the EEPROM
    while(1) {
        len = EEPROM.read(eeprom_offset_read);
        ++eeprom_offset_read;
        aci_cmd->buffer[0] = len;

        for (uint8_t i=1; i<=len; ++i) {
            aci_cmd->buffer[i] = EEPROM.read(eeprom_offset_read);
            ++eeprom_offset_read;
        }

        //Send the ACI Write Dynamic Data
        if (!hal_aci_tl_send(aci_cmd)) {
            Serial.println(F("bond_data_restore: Cmd Q Full"));
            return ACI_STATUS_ERROR_INTERNAL;
        }

        //Spin in the while loop waiting for an event
        while (1) {
            if (lib_aci_event_get(aci_stat, aci_data)) {
                aci_evt = &aci_data->evt; 

                if (ACI_EVT_CMD_RSP != aci_evt->evt_opcode) {
                    //Got something other than a command response evt -> Error
                    Serial.print(F("bond_data_restore: Expected cmd rsp evt. Got: 0x"));           
                    Serial.println(aci_evt->evt_opcode, HEX);
                    return ACI_STATUS_ERROR_INTERNAL;
                } else {
                    --write_dyn_num_msgs;

                    //ACI Evt Command Response
                    if (ACI_STATUS_TRANSACTION_COMPLETE == aci_evt->params.cmd_rsp.cmd_status) {
                        //Set the state variables correctly
                        *bonded_first_time_state = false;
                        aci_stat->bonded = ACI_BOND_STATUS_SUCCESS;

                        delay(10);

                        return ACI_STATUS_TRANSACTION_COMPLETE;
                    }
                    if (0 >= write_dyn_num_msgs) {
                        //should have returned earlier
                        return ACI_STATUS_ERROR_INTERNAL;
                    }
                    if (ACI_STATUS_TRANSACTION_CONTINUE == aci_evt->params.cmd_rsp.cmd_status) {            
                        //break and write the next ACI Write Dynamic Data
                        break;
                    }
                }
            }
        }
    }
}


/*
   This function is specific to the atmega328
   @params ACI Command Response Evt received from the Read Dynmaic Data
 */
void bond_data_store(aci_evt_t *evt) {
    static int eeprom_write_offset = 1;

    //Write it to non-volatile storage
    EEPROM.write( eeprom_write_offset, evt->len -2 );
    ++eeprom_write_offset;

    EEPROM.write( eeprom_write_offset, ACI_CMD_WRITE_DYNAMIC_DATA);
    ++eeprom_write_offset;

    for (uint8_t i=0; i< (evt->len-3); ++i) {
        EEPROM.write( eeprom_write_offset, evt->params.cmd_rsp.params.padding[i]);
        ++eeprom_write_offset;
    }
}

bool bond_data_read_store(aci_state_t *aci_stat, hal_aci_data_t* aci_cmd, hal_aci_evt_t* aci_data) {
    /*
       The size of the dynamic data for a specific Bluetooth Low Energy configuration
       is present in the ublue_setup.gen.out.txt generated by the nRFgo studio as "dynamic data size".
     */
    bool status = false;
    aci_evt_t * aci_evt = NULL;
    uint8_t read_dyn_num_msgs = 0;

    //Start reading the dynamic data 
    lib_aci_read_dynamic_data();
    ++read_dyn_num_msgs;

    while (1) {
        if (true == lib_aci_event_get(aci_stat, aci_data)) {
            aci_evt = &(aci_data->evt);

            if (ACI_EVT_CMD_RSP != aci_evt->evt_opcode ) {
                //Got something other than a command response evt -> Error
                status = false;
                break;
            }

            if (ACI_STATUS_TRANSACTION_COMPLETE == aci_evt->params.cmd_rsp.cmd_status) {
                //Store the contents of the command response event in the EEPROM 
                //(len, cmd, seq-no, data) : cmd ->Write Dynamic Data so it can be used directly
                bond_data_store(aci_evt);

                //Set the flag in the EEPROM that the contents of the EEPROM is valid
                EEPROM.write(0, 0x80|read_dyn_num_msgs );
                //Finished with reading the dynamic data
                status = true;

                break;
            }

            if (!(ACI_STATUS_TRANSACTION_CONTINUE == aci_evt->params.cmd_rsp.cmd_status)) {
                //We failed the read dymanic data
                //Set the flag in the EEPROM that the contents of the EEPROM is invalid
                EEPROM.write(0, 0x00);

                status = false;
                break;
            } else {
                //Store the contents of the command response event in the EEPROM 
                // (len, cmd, seq-no, data) : cmd ->Write Dynamic Data so it can be used directly when re-storing the dynamic data
                bond_data_store(aci_evt);

                //Read the next dynamic data message
                lib_aci_read_dynamic_data();
                ++read_dyn_num_msgs;
            }

        }
    }  
    return status;  
}