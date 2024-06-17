#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Text input anonymous structure */
typedef struct Wendigo_TextInput Wendigo_TextInput;
typedef void (*Wendigo_TextInputCallback)(void* context);
typedef bool (*Wendigo_TextInputValidatorCallback)(const char* text, FuriString* error, void* context);

/** Allocate and initialize text input 
 * 
 * This text input is used to enter string
 *
 * @return     Wendigo_TextInput instance
 */
Wendigo_TextInput* uart_hex_input_alloc();

/** Deinitialize and free text input
 *
 * @param      uart_text_input  Wendigo_TextInput instance
 */
void uart_hex_input_free(Wendigo_TextInput* uart_text_input);

/** Clean text input view Note: this function does not free memory
 *
 * @param      uart_text_input  Wendigo_TextInput instance
 */
void uart_hex_input_reset(Wendigo_TextInput* uart_text_input);

/** Get text input view
 *
 * @param      uart_text_input  Wendigo_TextInput instance
 *
 * @return     View instance that can be used for embedding
 */
View* uart_hex_input_get_view(Wendigo_TextInput* uart_text_input);

/** Set text input result callback
 *
 * @param      uart_text_input     Wendigo_TextInput instance
 * @param      callback            callback fn
 * @param      callback_context    callback context
 * @param      text_buffer         pointer to YOUR text buffer, that we going
 *                                 to modify
 * @param      text_buffer_size    YOUR text buffer size in bytes. Max string
 *                                 length will be text_buffer_size-1.
 * @param      clear_default_text  clear text from text_buffer on first OK
 *                                 event
 */
void uart_hex_input_set_result_callback(
    Wendigo_TextInput* uart_text_input,
    Wendigo_TextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text);

void uart_hex_input_set_validator(
    Wendigo_TextInput* uart_text_input,
    Wendigo_TextInputValidatorCallback callback,
    void* callback_context);

Wendigo_TextInputValidatorCallback
    uart_hex_input_get_validator_callback(Wendigo_TextInput* uart_text_input);

void* uart_hex_input_get_validator_callback_context(Wendigo_TextInput* uart_text_input);

/** Set text input header text
 *
 * @param      uart_text_input  Wendigo_TextInput instance
 * @param      text        text to be shown
 */
void uart_hex_input_set_header_text(Wendigo_TextInput* uart_text_input, const char* text);

#ifdef __cplusplus
}
#endif
