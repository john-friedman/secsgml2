import copy

def calculate_documents_locations_in_tar(metadata):
    # Step 1: Add placeholder byte positions to get accurate size (10-digit padded)
    placeholder_metadata = copy.deepcopy(metadata)

    doc_key = 'documents'
    size_key = 'secsgml_size_bytes'
    start_key = 'secsgml_start_byte'
    end_key = 'secsgml_end_byte'

    document_length = len(metadata[doc_key])
    
    for file_num in range(document_length):
        placeholder_metadata[doc_key][file_num][start_key] = "9999999999"  # 10 digits
        placeholder_metadata[doc_key][file_num][end_key] = "9999999999"  # 10 digits

    # Step 2: Calculate size with placeholders
    metadata_size = len(placeholder_metadata)
    
    # Step 3: Now calculate actual positions using this size
    current_pos = 512 + metadata_size
    current_pos += (512 - (current_pos % 512)) % 512
    
    # Step 4: Calculate real positions and update original metadata (10-digit padded)
    for file_num in range(document_length):
        size_bytes = metadata[doc_key][file_num][size_key]  # Get size from original metadata
        start_byte = current_pos + 512
        end_byte = start_byte + size_bytes

        metadata[doc_key][file_num][start_key] = f"{start_byte:010d}"  # Update original metadata
        metadata[doc_key][file_num][end_key] = f"{end_byte:010d}"     # Update original metadata
        
        file_total_size = 512 + size_bytes
        padded_size = file_total_size + (512 - (file_total_size % 512)) % 512
        current_pos += padded_size
    
    return metadata
