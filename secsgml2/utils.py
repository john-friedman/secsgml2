import json
import copy

def calculate_documents_locations_in_tar(metadata):
    placeholder_metadata = copy.deepcopy(metadata)

    doc_key = 'documents'
    size_key = 'secsgml_size_bytes'
    start_key = 'secsgml_start_byte'
    end_key = 'secsgml_end_byte'

    document_length = len(metadata[doc_key])
    
    for file_num in range(document_length):
        placeholder_metadata[doc_key][file_num][start_key] = "9999999999"
        placeholder_metadata[doc_key][file_num][end_key] = "9999999999"

    # FIX: serialize to JSON first, then measure byte length
    placeholder_json = json.dumps(placeholder_metadata).encode('utf-8')
    metadata_size = len(placeholder_json)
    
    current_pos = 512 + metadata_size
    current_pos += (512 - (current_pos % 512)) % 512
    
    for file_num in range(document_length):
        size_bytes = metadata[doc_key][file_num][size_key]
        start_byte = current_pos + 512
        end_byte = start_byte + size_bytes

        metadata[doc_key][file_num][start_key] = f"{start_byte:010d}"
        metadata[doc_key][file_num][end_key] = f"{end_byte:010d}"
        
        file_total_size = 512 + size_bytes
        padded_size = file_total_size + (512 - (file_total_size % 512)) % 512
        current_pos += padded_size
    
    return metadata