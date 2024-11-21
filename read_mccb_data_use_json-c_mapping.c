#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <termios.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <json-c/json.h>


#define DEBUG_MODE 1 // Set to 1 to enable custom debug statements, 0 to disable

#if DEBUG_MODE
#define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__) // The `##__VA_ARGS__` allows for an empty argument list
#else
#define DEBUG_PRINT(fmt, ...) // Do nothing
#endif

#define UART_DEVICE "/dev/ttyAMA0"
#define BAUD_RATE 115200
#define NB_STRING 6
#define STRING_LEN 5
#define holding_register_address 40000

static uint8_t response_data[256]="\0";

bool data_length_status = true;  // get the status of data_length variable.

int res=0;

uint8_t register_count=0;

uint8_t store_data_length_data=0;

uint8_t total_number_of_register_bytes = 0;

int data_length_decimal=0;

bool holding_register_40001_location_status = true;


int uart_fd;

struct termios uart_settings;

// Prepare ResponseData (address, function code, data, and CRC)
// uint8_t responseData[256]; // Max size for response

static uint8_t dataSize = 0; // Holds the actual response size (excluding CRC)


uint16_t calculateCRC(uint8_t *buf, int len)
{
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < len; pos++)
    {
        crc ^= (uint16_t)buf[pos]; // XOR byte into least sig. byte of crc
        for (int i = 8; i != 0; i--)
        { // Loop over each bit
            if ((crc & 0x0001) != 0)
            {              // If the LSB is set
                crc >>= 1; // Shift right and XOR 0xA001
                crc ^= 0xA001;
            }
            else           // Else LSB is not set
                crc >>= 1; // Just shift right
        }
    }
    return crc;
}

uint16_t hex_to_decimal(uint16_t hex_value)
{  
   if (hex_value <= 0x0F) {  
      return hex_value;  
   } else {  
      char hex_str[3];  
      sprintf(hex_str, "%02x", hex_value);  
      return atoi(hex_str);  
   }  
}  

void reset_global_variable (void)
{

    register_count=0;

    store_data_length_data=0;

    total_number_of_register_bytes = 0;

    data_length_status = true;

    res=0;

    holding_register_40001_location_status = true;

}
  

int writing_response_data(void)
{

    // Calculate CRC for the response
    uint16_t crc = calculateCRC(response_data, dataSize);
    response_data[dataSize++] = crc & 0xFF;        // CRC low byte
    response_data[dataSize++] = (crc >> 8) & 0xFF; // CRC high byte

    // Print ResponseData in hex format

    // printf("....................datasize=%d\n",dataSize);
    printf("ResponseData (hex): ");
    for (int i = 0; i < dataSize; i++)
    {
        printf("%02X ", response_data[i]);
    }
    printf("\n"); 

    DEBUG_PRINT("..................Datasize information=%d\n",dataSize);
   // Write the responseData array through the UART
   int bytes_written = write(uart_fd,response_data,dataSize);
   if (bytes_written != dataSize) {
      perror("Error writing to UART");
     return 1;
    }  
}

void print_json_values(json_object *val,int numberOfRegistersCountDecimal) 
{  
    int i=0;

    // Get the length of the array (we expect an array with two elements)
    int array_length = json_object_array_length(val);

    if (array_length != 2) {
        printf("Unexpected array size. Expected 2 elements.\n");
        return;
    }

    // Get the data length (first field)
    json_object *length_obj = json_object_array_get_idx(val, 0);
    const char *data_length = json_object_get_string(length_obj);
    DEBUG_PRINT("Data Length in string: %s\n", data_length);
    data_length_decimal = atoi(data_length);
   // int data_length_decimal = hex_to_decimal(data_length);  
   // printf("@@@@Decimal value: %d\n", data_length_decimal);  

    // Get the large decimal value (second field)
    json_object *value_obj = json_object_array_get_idx(val, 1);
    const char *decimal_value = json_object_get_string(value_obj);
    DEBUG_PRINT("Large Decimal Value in string: %s\n", decimal_value);


    if(data_length_decimal == 12 && data_length_status )
    {
      
        DEBUG_PRINT("...................statement-1.................\n");
        total_number_of_register_bytes = numberOfRegistersCountDecimal * 2;
        data_length_status = false;
        store_data_length_data = 12;
        // Store the number of data bytes in the response
        response_data[dataSize++] = total_number_of_register_bytes;
    }
    else if((data_length_decimal == 4 || data_length_decimal == 2) && (data_length_status))
    {
        DEBUG_PRINT("..................statement-2.......................\n");
        total_number_of_register_bytes = data_length_decimal * numberOfRegistersCountDecimal;
        data_length_status = false;
        store_data_length_data = data_length_decimal;
        // Store the number of data bytes in the response
        response_data[dataSize++] = numberOfRegistersCountDecimal * 2;
    }

    DEBUG_PRINT("......................statement-3..........................\n");
    res  += data_length_decimal; 
    
    DEBUG_PRINT("...................statement-3-1..........total_number_of_register_bytes=%d\n",total_number_of_register_bytes);
    DEBUG_PRINT("...................statement-3-2..........res=%d\n",res);
    
    if(((data_length_decimal == 4 || data_length_decimal == 2) && (total_number_of_register_bytes >= res)) && (store_data_length_data == data_length_decimal))
    {
        
        DEBUG_PRINT("......................statement-4..........................\n");
        DEBUG_PRINT("......................statement-4-0...dataSize=%d........\n",dataSize);
        DEBUG_PRINT("......................statement-4-0................data_length_decimal=%d\n",data_length_decimal);
        uint32_t value = atoi(decimal_value);
        for(i=0,dataSize=dataSize+data_length_decimal;i<data_length_decimal;i++)
        {
            DEBUG_PRINT(".................statement -4-1....................\n");
            response_data[--dataSize] = (value >> (i*8)) & 0xFF;
            DEBUG_PRINT("..........................response_data[%d]=%02x\n",dataSize,response_data[dataSize]);
        }
        DEBUG_PRINT(".................statement -4-2....................\n");
        DEBUG_PRINT("......................statement-4-2...dataSize=%d........\n",dataSize);
        dataSize = dataSize+data_length_decimal;
        ++register_count;
        DEBUG_PRINT("......................statement-4-2-0...dataSize=%d........\n",dataSize);
        DEBUG_PRINT(".................statement -4-3....................\n");

       /* for (i = 0; i < dataSize; i++) 
        {  
            printf("%02x\t", response_data[i]);  
        } */ 
     
    }
    if(((data_length_decimal == 12) && (total_number_of_register_bytes >= res)) && (store_data_length_data == data_length_decimal))
    {
        uint32_t data[256];
        char chunk[STRING_LEN + 1] = {0};  // Buffer for a 5-character chunk (+1 for null-terminator)
    
       DEBUG_PRINT("......................statement-5..........................\n");
    // Process the decimal string and convert it to hexadecimal
    for (int i = 0; i < NB_STRING; i++) {
        // Get 5 characters at a time
        strncpy(chunk, decimal_value + i * STRING_LEN, STRING_LEN);
        chunk[STRING_LEN] = '\0';  // Null-terminate the chunk

        // Convert the chunk to a decimal integer
        unsigned int decimal_val = atoi(chunk);
        
        // Convert the decimal integer to a hexadecimal value and store it in response_data
        data[i] = decimal_val;

        // Print the chunk, the decimal value, and its hexadecimal equivalent
        //printf("Chunk %d: %s (Decimal: %u, Hex: %04x)\n", i+1, chunk, decimal_val, data[i]);
    }
     // Now store the data values into response_data by splitting each value into two bytes
    for (int i = 0; i < NB_STRING; i++) 
    {
        DEBUG_PRINT("......................statement-6..........................\n");
        // Store the high byte and low byte of each 16-bit value in response_data
        response_data[dataSize++] = (data[i] >> 8) & 0xFF;  // High byte
        response_data[dataSize++] = data[i] & 0xFF;         // Low byte
    }
    ++register_count;


    
    }
}   

int parseJsonFrame (int registerAddressDecimal,int numberOfRegistersCountDecimal)
{
    volatile int  found_address = 0;  // Flag to indicate when the address is found
    int print_data_length = 1; // Ensure data length is printed only once
    int Register_Address_json = 0;
    int  Register_Address_json_decimal = 0;
   
     // Open the JSON file
    FILE *file = fopen("data.json", "r");
    if (file == NULL) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    // Read the entire file into a string
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *json_string = malloc(file_size + 1);
    fread(json_string, 1, file_size, file);
    json_string[file_size] = '\0'; // Null-terminate the string
    fclose(file);

    // Parse the JSON string
    json_object *jobj = json_tokener_parse(json_string);
    free(json_string); // Free the allocated memory for the JSON string

    // Check if the JSON object is valid
    if (jobj == NULL) {
        printf("Error parsing JSON\n");
        return EXIT_FAILURE;
    }

    // Iterate through the JSON object
    json_object_object_foreach(jobj, key, val) {
   
    //print_json_values(val);     // Print the values (length and large decimal number)

  
    DEBUG_PRINT("Key: %s\n", key);  // Print the key (e.g., "30300")
    Register_Address_json = atoi(key);
    DEBUG_PRINT("...........................register_address_json=%d\n",Register_Address_json);
    Register_Address_json_decimal = Register_Address_json - 30000;
    if(!found_address)
    {
      
        if(Register_Address_json_decimal == registerAddressDecimal)
        {
            found_address = 1;  // Mark that the address has been found

        }
       
    }
 
    if (found_address) 
    {
            // Only print values after finding the key, no key
            print_json_values(val,numberOfRegistersCountDecimal);

            DEBUG_PRINT("......total number of register bytes:%d\n",total_number_of_register_bytes);
            DEBUG_PRINT("..........total register count:%d\n",store_data_length_data*register_count);
            if(total_number_of_register_bytes == (store_data_length_data*register_count))
            {
               int total_bytes_written =  writing_response_data();

               if (total_bytes_written == dataSize) 
               {
                    printf("Total %d bytes of ResponseData successfully written to UART\n",total_bytes_written);
                    reset_global_variable();               
                    return 0;  // Indicate Success
               }
               else
               {
                    printf("Error writing response data to UART\n");
                    reset_global_variable();
                    return 1;  // Return error status
               }

                
            }
            if((total_number_of_register_bytes < res) || (store_data_length_data != data_length_decimal))
            {
              //  printf("Given Register count number is invalid\n");

                reset_global_variable();

                return 3;
            }
    }
    
 }

    // Check if the key was found or not
    if (!found_address) {
       // printf("Key '%d' not found in the JSON data.\n", registerAddressDecimal);
        printf("Key not found in the JSON data.\n");
        return 2;
    }
    DEBUG_PRINT("....................Register_Address_json_decimal:%d\n",Register_Address_json_decimal);
    if(Register_Address_json_decimal == 563 )
    {
      //  printf("\n $$$$$$$$$$$$$$ The registre count is invalid $$$$$$$$$$$$$$$$$$$$$$\n ");
        reset_global_variable();
        return 3;
    }

    // Free the JSON object
    json_object_put(jobj);
}

void  fetching_holding_register_Data (json_object *val,int holding_register_address_json,int numberOfRegistersCountDecimal)
{
    int i=0;

    // Get the length of the array (we expect an array with two elements)
    int array_length = json_object_array_length(val);

    if (array_length != 2) {
        printf("Unexpected array size. Expected 2 elements.\n");
        return;
    }

    // Get the data length (first field)
    json_object *length_obj = json_object_array_get_idx(val, 0);
    const char *data_length = json_object_get_string(length_obj);
    DEBUG_PRINT("Data Length in string: %s\n", data_length);
    data_length_decimal = atoi(data_length);
   // int data_length_decimal = hex_to_decimal(data_length);  
   // printf("@@@@Decimal value: %d\n", data_length_decimal);  

    // Get the large decimal value (second field)
    json_object *value_obj = json_object_array_get_idx(val, 1);
    const char *decimal_value = json_object_get_string(value_obj);
    DEBUG_PRINT("Large Decimal Value in string: %s\n", decimal_value);


    if(data_length_decimal == 8 && data_length_status )
    {
      
        DEBUG_PRINT("...................statement-1.................\n");
        total_number_of_register_bytes = numberOfRegistersCountDecimal * 2;
        data_length_status = false;
        store_data_length_data = data_length_decimal;
        // Store the number of data bytes in the response
        response_data[dataSize++] = total_number_of_register_bytes;
    }
    else if((data_length_decimal == 2) && (data_length_status))
    {
        DEBUG_PRINT("..................statement-2.......................\n");
        total_number_of_register_bytes = data_length_decimal * numberOfRegistersCountDecimal;
        data_length_status = false;
        store_data_length_data = data_length_decimal;
        // Store the number of data bytes in the response
        response_data[dataSize++] = numberOfRegistersCountDecimal * 2;
    }
    DEBUG_PRINT("......................statement-3..........................\n");
    res  += data_length_decimal; 
    
    DEBUG_PRINT("...................statement-3-1..........total_number_of_register_bytes=%d\n",total_number_of_register_bytes);
    DEBUG_PRINT("...................statement-3-2..........res=%d\n",res);

     if((data_length_decimal == 2) && (total_number_of_register_bytes >= res) && (holding_register_address_json >= 40001 && holding_register_address_json <= 40014))
    {
        
        DEBUG_PRINT("......................statement-4..........................\n");
        DEBUG_PRINT("......................statement-4-0...dataSize=%d........\n",dataSize);
        DEBUG_PRINT("......................statement-4-0................data_length_decimal=%d\n",data_length_decimal);
        uint32_t value = atoi(decimal_value);
        for(i=0,dataSize=dataSize+data_length_decimal;i<data_length_decimal;i++)
        {
            DEBUG_PRINT(".................statement -4-1....................\n");
            response_data[--dataSize] = (value >> (i*8)) & 0xFF;
            DEBUG_PRINT("..........................response_data[%d]=%02x\n",dataSize,response_data[dataSize]);
        }
        DEBUG_PRINT(".................statement -4-2....................\n");
        DEBUG_PRINT("......................statement-4-2...dataSize=%d........\n",dataSize);
        dataSize = dataSize+data_length_decimal;
        ++register_count;
        holding_register_40001_location_status = false;
        DEBUG_PRINT(".......holding_register_40001_location_status.......................%d\n",holding_register_40001_location_status);
        DEBUG_PRINT("......................statement-4-2-0...dataSize=%d........\n",dataSize);
        DEBUG_PRINT(".................statement -4-3....................\n");

       /* for (i = 0; i < dataSize; i++) 
        {  
            printf("%02x\t", response_data[i]);  
        } */ 
     
    }
     
    else if(((data_length_decimal == 2) && (total_number_of_register_bytes >= res)) && (holding_register_40001_location_status))
    {
        DEBUG_PRINT(".......holding_register_40001_location_status.......................%d\n",holding_register_40001_location_status);
        DEBUG_PRINT("......................statement-4..........................\n");
        DEBUG_PRINT("......................statement-4-0...dataSize=%d........\n",dataSize);
        DEBUG_PRINT("......................statement-4-0................data_length_decimal=%d\n",data_length_decimal);
        uint32_t value = atoi(decimal_value);
        for(i=0,dataSize=dataSize+data_length_decimal;i<data_length_decimal;i++)
        {
            DEBUG_PRINT(".................statement -4-1....................\n");
            response_data[--dataSize] = (value >> (i*8)) & 0xFF;
            DEBUG_PRINT("..........................response_data[%d]=%02x\n",dataSize,response_data[dataSize]);
        }
        DEBUG_PRINT(".................statement -4-2....................\n");
        DEBUG_PRINT("......................statement-4-2...dataSize=%d........\n",dataSize);
        dataSize = dataSize+data_length_decimal;
        ++register_count;
        DEBUG_PRINT("......................statement-4-2-0...dataSize=%d........\n",dataSize);
        DEBUG_PRINT(".................statement -4-3....................\n");
    }
     else if(((data_length_decimal == 8) && (total_number_of_register_bytes >= res)) && (store_data_length_data == data_length_decimal))
    {
        uint32_t data[256];
        int chunk_sizes[] = {4, 5, 4, 5};
        int num_chunks = sizeof(chunk_sizes) / sizeof(chunk_sizes[0]);
        int str_index = 0;
        
       DEBUG_PRINT("......................statement-5..........................\n");
    // Process the decimal string and convert it to hexadecimal
    for (int i = 0; i < num_chunks; i++) {

        int chunk_size = chunk_sizes[i];
        char chunk[chunk_size + 1];
        
        strncpy(chunk, decimal_value + str_index, chunk_size);
        chunk[chunk_size] = '\0';
        // Convert the chunk to a decimal integer
        unsigned int decimal_val = atoi(chunk);
        
        // Convert the decimal integer to a hexadecimal value and store it in response_data
        data[i] = decimal_val;

        // Print the chunk, the decimal value, and its hexadecimal equivalent
        //printf("Chunk %d: %s (Decimal: %u, Hex: %04x)\n", i+1, chunk, decimal_val, data[i]);

        str_index += chunk_size;
    }
     // Now store the data values into response_data by splitting each value into two bytes
    for (int i = 0; i < num_chunks; i++) 
    {
        DEBUG_PRINT("......................statement-6..........................\n");
        // Store the high byte and low byte of each 16-bit value in response_data
        response_data[dataSize++] = (data[i] >> 8) & 0xFF;  // High byte
        response_data[dataSize++] = data[i] & 0xFF;         // Low byte
    }
    ++register_count;


    
    }
    
}
int read_holding_register_data (json_object *jobj,int registerAddressDecimal,int numberOfRegistersCountDecimal)
{
    int holding_register_address_json = 0;
    int holding_register_address_json_decimal = 0;

    int found_holding_register_address = 0;

    // Iterate through the JSON object
    json_object_object_foreach(jobj, key, val)
    {
        DEBUG_PRINT("Key: %s\n", key);  // Print the key (e.g., "30300")
        holding_register_address_json = atoi(key);
        DEBUG_PRINT("...........................register_address_json=%d\n",holding_register_address_json);
        holding_register_address_json_decimal = holding_register_address_json - holding_register_address;
        DEBUG_PRINT("........................found.holding..register_address_json=%d\n",holding_register_address_json);


        if(!found_holding_register_address)
        {
      
            if(holding_register_address_json_decimal == registerAddressDecimal)
            {
                found_holding_register_address = 1;  // Mark that the address has been found
            }
       
        }

         if (found_holding_register_address) 
        {
            // Only print values after finding the key, no key
            fetching_holding_register_Data(val,holding_register_address_json,numberOfRegistersCountDecimal);



            DEBUG_PRINT("......total number of register bytes:%d\n",total_number_of_register_bytes);
            DEBUG_PRINT("..........total register count:%d\n",store_data_length_data*register_count);
            if(total_number_of_register_bytes == (store_data_length_data*register_count))
            {
               int total_bytes_written =  writing_response_data();

               if (total_bytes_written == dataSize) 
               {
                    printf("Total %d bytes of ResponseData successfully written to UART\n",total_bytes_written);
                    reset_global_variable();               
                    return 0;  // Indicate Success
               }
               else
               {
                    printf("Error writing response data to UART\n");
                    reset_global_variable();
                    return 1;  // Return error status
               }

              
                
            }
            if((total_number_of_register_bytes < res) || (store_data_length_data != data_length_decimal))
            {
              //  printf("Given Register count number is invalid\n");

                reset_global_variable();

                return 3;
            }
        }
    }  
     // Check if the key was found or not
    if (!found_holding_register_address) {
       // printf("Key '%d' not found in the JSON data.\n", registerAddressDecimal);
        printf("Key not found in the JSON data.\n");
        return 2;
    } 

     if(holding_register_address_json_decimal == 300 )
    {
      //  printf("\n $$$$$$$$$$$$$$ The registre count is invalid $$$$$$$$$$$$$$$$$$$$$$\n ");
        reset_global_variable();
        return 3;
    }

    // Free the JSON object
    json_object_put(jobj);

}



int read_json_data (int registerAddressDecimal,int numberOfRegistersCountDecimal)
{
       // Open the JSON file
    FILE *file = fopen("data-1.json", "r");
    if (file == NULL) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    // Read the entire file into a string
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *json_string = malloc(file_size + 1);
    fread(json_string, 1, file_size, file);
    json_string[file_size] = '\0'; // Null-terminate the string
    fclose(file);

    // Parse the JSON string
    json_object *jobj = json_tokener_parse(json_string);
    free(json_string); // Free the allocated memory for the JSON string

     // Check if the JSON object is valid
    if (jobj == NULL) {
        printf("Error parsing JSON\n");
        return EXIT_FAILURE;
    }
   int status = read_holding_register_data(jobj,registerAddressDecimal,numberOfRegistersCountDecimal);
   return status;

}
int update_json_value(const char *filename, const char *key, const char *new_value) {
    
    DEBUG_PRINT("filename is: %s\n",filename);
    DEBUG_PRINT("key is :%s\n",key);
    DEBUG_PRINT("value is :%s\n",new_value);
    
    // Open the JSON file
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    // Read the entire file into a string
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *data = malloc(length);
    if (!data) {
        fclose(file);
        perror("Failed to allocate memory");
        return 1;
    }
    
    fread(data, 1, length, file);
    fclose(file);

    // Parse the JSON data
    struct json_object *parsed_json = json_tokener_parse(data);
    free(data); // Free the allocated memory for JSON string

    if (!parsed_json) {
        fprintf(stderr, "Failed to parse JSON\n");
        return 1;
    }

    // Get the value associated with the key
    struct json_object *value_array;
    if (json_object_object_get_ex(parsed_json, key, &value_array)) {
        // Update the second field (index 1) of the array
        json_object_array_put_idx(value_array, 1, json_object_new_string(new_value));
        
        // Write back to file
        file = fopen(filename, "w");
        if (!file) {
            perror("Failed to open file for writing");
            json_object_put(parsed_json); // Free JSON object
            return 1;
        }
        
        // Convert modified JSON object back to string and write it to file
        const char *modified_data = json_object_to_json_string(parsed_json);
        fprintf(file, "%s", modified_data);
        
        fclose(file);
    } else {
        fprintf(stderr, "Key not found in JSON\n");
        return 1;
    }

    // Clean up
    json_object_put(parsed_json); // Free JSON object
    return 0;
}


// Function to process the received Modbus frame
int parseModbusFrame(uint8_t *modbusData, int len)
{
     dataSize = 0;
    // Ensure the minimum frame size (Address + Function Code + CRC = 4 bytes)
    if (len < 4)
    {
        printf("Invalid frame size\n");
        return 0;
    }

    // Extract Address
    uint8_t slave_id = modbusData[0];
    printf("Slave ID: 0x%02X\n", slave_id);

    // Extract Function Code
    uint8_t functionCode = modbusData[1];
    printf("Function Code: 0x%02X\n", functionCode);

    // Check CRC
    uint16_t receivedCRC = (modbusData[len - 2] | (modbusData[len - 1] << 8));
    uint16_t calculatedCRC = calculateCRC(modbusData, len - 2);

    DEBUG_PRINT("receivedCRC = %x\n", receivedCRC);
    DEBUG_PRINT("calculatedCRC = %x\n", calculatedCRC);

    if (receivedCRC != calculatedCRC)
    {
        printf("CRC mismatch! Received: 0x%04X, Calculated: 0x%04X\n", receivedCRC, calculatedCRC);
        return 1;
    }
    else
    {
        printf("CRC check passed\n");
    }

    response_data[dataSize++] = slave_id;      // Address
    response_data[dataSize++] = functionCode; // Function code

    // Process data based on function code
    switch (functionCode)
    {
    case 0x04: // Read the data from Input Registers
        printf("Read thd Data from Input Registers:\n");

        uint16_t registerAddress = (modbusData[2] << 8) | modbusData[3];
        uint16_t numberOfRegisters = (modbusData[4] << 8) | modbusData[5];

        uint16_t registerAddressDecimal = (registerAddress & 0xFF) + ((registerAddress >> 8) * 256);

       // uint16_t numberOfRegistersCountDecimal = (numberOfRegisters & 0xFF) + ((numberOfRegisters >> 8) * 256);
        uint16_t  numberOfRegistersCountDecimal = hex_to_decimal(numberOfRegisters);  

        DEBUG_PRINT("registerAddressDecimal=%d\n", registerAddressDecimal);
        DEBUG_PRINT("numberOfRegistersCountDecimal=%d\n", numberOfRegistersCountDecimal);

        if(registerAddressDecimal >= 0 && registerAddressDecimal <= 563)
        {
            int status = parseJsonFrame(registerAddressDecimal,numberOfRegistersCountDecimal);
            if(status == 1)
            {
                printf("parseJsonFrame status failed\n");
            }
            else if (status == 2)
            {
                printf("\n ...............There is no specific address location within the Address Range.................... \n");
            }
            else if (status == 3)
            {
                 printf("\n @@@@@@@@@@@@@@@@   Given Register count number is invalid @@@@@@@@@@@@@@@@@@@ \n");
            }
            else 
            {
                printf("Data processed and response sent successfully.\n");
            }
        }
        else
        {
            printf("\n **************************** Register Address is out of Range ******************************* \n");
            return 0;
        }
        break;
    case 0x06: // write the Data
        printf("\n...............Write the Data into Holding Registers................\n");
    
        registerAddress = (modbusData[2] << 8) | modbusData[3];  // For store the two bytes of Register Address Data

        uint16_t data = (modbusData[4] << 8) | modbusData[5]; // For store the one byte of Data

        DEBUG_PRINT("Register Adress Data..............%02x\n",registerAddress);

        DEBUG_PRINT("Data....................%02x\n",data);

     //   registerAddressDecimal = (registerAddress & 0xFF) + ((registerAddress >> 8) * 256);

     //   uint16_t decimal_value_data = (data & 0xFF) + ((data >> 8) & 0xFF);
     //   uint16_t decimal_value_data = data;

    //    DEBUG_PRINT("Register Adress Data in Decimal............%d\n",registerAddressDecimal);

     //   DEBUG_PRINT("Data in Decimal...................%d\n",decimal_value_data);

      

        response_data[dataSize++] = modbusData[2];
        response_data[dataSize++] = modbusData[3];
        response_data[dataSize++] = modbusData[4];
        response_data[dataSize++] = modbusData[5];
          
        if(registerAddress >= 1 && registerAddress <= 300)
        {
                const char *filename = "data-2.json";
                char key_string[20],value_string[20];
                int  Address = registerAddress + 40000;
                sprintf(key_string,"%d",Address);
                sprintf(value_string,"%d",data);      
                int res = update_json_value(filename,key_string,value_string);
                if(res == 0)
                {


                    int total_bytes_written =  writing_response_data();

                    if (total_bytes_written == dataSize) 
                    {
                        printf("Total %d bytes of ResponseData successfully written to UART\n",total_bytes_written);
                        reset_global_variable();               
                        return 0;  // Indicate Success
                    }
                    else
                    {
                        printf("Error writing response data to UART\n");
                        reset_global_variable();
                        return 1;  // Return error status
                    }
                }

        }
        break;

    case 0x10:

        printf("\n .....................Write the Data into Mulitple Holding Registers .......................... \n");

        char key_string[20]="\0",value_string[40]="\0", str[20]="\0";
        const char *filename = "data-2.json";
        int index = 6;
       
        registerAddress = (modbusData[2] << 8) | modbusData[3];  // For store the two bytes of Register Address Data
        numberOfRegisters = (modbusData[4] << 8) | modbusData[5];
       

        response_data[dataSize++] = modbusData[2];
        response_data[dataSize++] = modbusData[3];
        response_data[dataSize++] = modbusData[4];
        response_data[dataSize++] = modbusData[5];

        int  Address = registerAddress + 40000;
        sprintf(key_string,"%d",Address);

        for(int i=0;i<numberOfRegisters;i++)
        {
           
             uint16_t data = (modbusData[index+0] << 8) | modbusData[index+1]; // For store the one byte of Data
             sprintf(str,"%d",data);
             strcat(value_string,str); 
           //  response_data[dataSize++] = modbusData[index+0];
           //  response_data[dataSize++] = modbusData[index+1];
             index = index + 2;

        }
         update_json_value(filename,key_string,value_string);
         int total_bytes_written =  writing_response_data();

        if (total_bytes_written == dataSize) 
        {
            printf("Total %d bytes of ResponseData successfully written to UART\n",total_bytes_written);
            reset_global_variable();               
            return 0;  // Indicate Success
        }
        else
        {
            printf("Error writing response data to UART\n");
            reset_global_variable();
            return 1;  // Return error status
        }



        break;
    case 0x03:
        printf("\n ...................Read the Data from Holding Registers ............................\n"); 

        registerAddress = (modbusData[2] << 8) | modbusData[3];  // For store the two bytes of Register Address Data

        numberOfRegisters = (modbusData[4] << 8) | modbusData[5]; // For store the one byte of Data

        registerAddressDecimal = (registerAddress & 0xFF) + ((registerAddress >> 8) * 256);

        numberOfRegistersCountDecimal = (numberOfRegisters & 0xFF) + ((numberOfRegisters >> 8) * 256);

        DEBUG_PRINT("registerAddressDecimal=%d\n", registerAddressDecimal);
        DEBUG_PRINT("numberOfRegistersCountDecimal=%d\n", numberOfRegistersCountDecimal);

        if(registerAddressDecimal >= 1 && registerAddressDecimal <= 300)
        {
            int status  = read_json_data(registerAddressDecimal,numberOfRegistersCountDecimal);
            if(status == 1)
            {
                printf("parseJsonFrame status failed\n");
            }
            else if (status == 2)
            {
                printf("\n ...............There is no specific address location within the Address Range.................... \n");
            }
            else if (status == 3)
            {
                 printf("\n @@@@@@@@@@@@@@@@   Given Register count number is invalid @@@@@@@@@@@@@@@@@@@ \n");
            }
            else 
            {
                printf("Data processed and response sent successfully.\n");
            }
        }
        else
        {
            printf("\n **************************** Register Address is out of Range ******************************* \n");
            return 0;
        }
   

    }


    
}


int main()
{

     uint8_t buffer[256];

     uart_fd = open(UART_DEVICE, O_RDWR | O_NOCTTY | O_SYNC);
     if (uart_fd < 0)
     {
        perror("Error opening UART device file");
        return 1;
     }

     struct termios options;
    tcgetattr(uart_fd, &options);

    // Set baud rate (adjust as needed)
    cfsetispeed(&options, BAUD_RATE);
    cfsetospeed(&options, BAUD_RATE);

    // Disable CR to LF conversion
   // options.c_iflag &= ~(ICRNL | INLCR);  // Disable input CR->NL and NL->CR translation
    options.c_iflag &= ~( IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR );
    options.c_oflag &= ~(OCRNL | ONLCR);  // Disable output CR->NL and NL->CR translation
    

    // Set UART for raw mode
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    options.c_oflag &= ~OPOST;                          // Raw output

    // Apply settings
    tcsetattr(uart_fd, TCSANOW, &options);
    

    /*while (1)
    {
        int len = read(uart_fd, buffer, 256);
        dataSize = 0;
        DEBUG_PRINT("..................................Initial dataSize after reset:%d\n",dataSize);
        if (len > 0)
        {
            printf("Received %d bytes: ", len);
            for (int i = 0; i < len; i++)
            {
                printf("0x%02x ", buffer[i]);
            }
            printf("\n");

            //int len = sizeof(buffer) / sizeof(buffer[0]);

            int res = parseModbusFrame(buffer, len);
        }
    }*/


    while (1)  
    {  
    int len = 0;  
    uint8_t buffer[256];  
    struct timeval timeout;  

    // Set timeout to 10ms  
    timeout.tv_sec = 0;  
    timeout.tv_usec = 90000;  

    // Read data from UART in chunks  
    while (1)  
    {  
        fd_set fds;  
        FD_ZERO(&fds);  
        FD_SET(uart_fd, &fds);  

        // Wait for data to be available or timeout  
        int ret = select(uart_fd + 1, &fds, NULL, NULL, &timeout);  
        if (ret <= 0)  
        {  
        break; // timeout or error  
        }  

        // Read data from UART  
        int chunk_len = read(uart_fd, buffer + len, 256 - len);  
        if (chunk_len <= 0)  
        {  
        break; // error or no more data  
        }  
        len += chunk_len;  
    }  

    if (len > 0)  
    {  
        printf("Received %d bytes: ", len);  
        for (int i = 0; i < len; i++)  
        {  
        printf("0x%02x ", buffer[i]);  
        }  
        printf("\n");  

        int res = parseModbusFrame(buffer, len);  
    }  
    }
    close(uart_fd);
    return 0;
}