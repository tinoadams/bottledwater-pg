/* JSON encoding for messages written to Kafka.
 *
 * The JSON format is defined by libavro's avro_value_to_json function, which
 * produces JSON as defined in the Avro spec:
 * https://avro.apache.org/docs/1.7.7/spec.html#json_encoding
 *
 * Examples:
 *
 *  * {"id": {"int": 1}} // an integer key
 *  * {"id": {"int": 3}, "title": {"string": "Man Bites Dog"}} // a row with two fields
 *
 * N.B. the output JSON does contain whitespace (as in the above examples), and
 * so may be rejected by strict JSON parsers.
 */

#include "json.h"

#include <avro.h>
#include <stdio.h>

int avro_bin_to_json(avro_schema_t schema,
        const void *val_bin, size_t val_len,
        char **val_out, size_t *val_len_out);


int json_encode_msg(table_metadata_t table,
        const void *key_bin, size_t key_len,
        char **key_out, size_t *key_len_out,
        const void *row_bin, size_t row_len,
        char **row_out, size_t *row_len_out) {
    int err;
    err = avro_bin_to_json(table->key_schema, key_bin, key_len, key_out, key_len_out);
    if (err) {
      fprintf(stderr, "json: error %s encoding key\n", avro_strerror());
      return err;
    }
    err = avro_bin_to_json(table->row_schema, row_bin, row_len, row_out, row_len_out);
    if (err) {
      fprintf(stderr, "json: error %s encoding row\n", avro_strerror());
      return err;
    }

    return 0;
}


int avro_bin_to_json(avro_schema_t schema,
        const void *val_bin, size_t val_len,
        char **val_out, size_t *val_len_out) {
    if (!val_bin) {
        *val_out = NULL;
        return 0;
    } else if (!schema) {
        /* got a value where we didn't expect one, and no schema to decode it */
        *val_out = NULL;
        return EINVAL;
    }

    avro_reader_t reader = avro_reader_memory(val_bin, val_len);

    avro_value_iface_t *iface = avro_generic_class_from_schema(schema);
    // TODO error handling?
    avro_value_t value;
    avro_generic_value_new(iface, &value);
    // TODO error handling?

    int err = avro_value_read(reader, &value);
    if (err) {
        avro_value_decref(&value);
        avro_value_iface_decref(iface);
        avro_reader_free(reader);
        return err;
    }

    err = avro_value_to_json(&value, 1, val_out);
    if (err) {
        avro_value_decref(&value);
        avro_value_iface_decref(iface);
        avro_reader_free(reader);
        return err;
    }

    *val_len_out = strlen(*val_out); // not including null terminator - to librdkafka it's just bytes

    avro_value_decref(&value);
    avro_value_iface_decref(iface);
    avro_reader_free(reader);

    return 0;
}
