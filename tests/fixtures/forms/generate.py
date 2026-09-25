#!/usr/bin/env python3
# Generate field.pdf with AcroForm Text + Checkbox fields for Epic 7.1
# Deterministic, no libreoffice/qpdf. Uses pymupdf (fitz) which is already a dependency.

import fitz  # pymupdf
import hashlib

def generate_field_pdf(output_path):
    """Generate a PDF with AcroForm fields: 2 text fields, 2 checkboxes."""
    doc = fitz.open()
    page = doc.new_page(width=612, height=792)  # Letter size

    # Add text labels
    page.insert_text((72, 100), "Name:", fontsize=12)
    page.insert_text((72, 180), "Email:", fontsize=12)
    page.insert_text((72, 260), "Subscribe to newsletter:", fontsize=12)
    page.insert_text((72, 300), "Agree to terms:", fontsize=12)

    # Create form fields
    # Text field: Name
    name_field = fitz.Widget()
    name_field.field_name = "Name"
    name_field.field_type = fitz.PDF_WIDGET_TYPE_TEXT
    name_field.rect = fitz.Rect(180, 90, 400, 120)
    name_field.field_value = ""
    name_field.max_len = 50
    name_field.text_font = "helv"
    name_field.text_fontsize = 12
    w = page.add_widget(name_field)
    # pymupdf does not serialize max_len on add_widget (verified: the AcroForm widget
    # ships without /MaxLen), so the key is written explicitly - the fixture must exercise
    # the /MaxLen path through the real dict walk, not only the synthetic IR.
    doc.xref_set_key(w.xref, "MaxLen", "50")

    # Text field: Email
    email_field = fitz.Widget()
    email_field.field_name = "Email"
    email_field.field_type = fitz.PDF_WIDGET_TYPE_TEXT
    email_field.rect = fitz.Rect(180, 170, 400, 200)
    email_field.field_value = ""
    email_field.max_len = 100
    email_field.text_font = "helv"
    email_field.text_fontsize = 12
    w = page.add_widget(email_field)
    doc.xref_set_key(w.xref, "MaxLen", "100")

    # Checkbox: Subscribe
    sub_field = fitz.Widget()
    sub_field.field_name = "Subscribe"
    sub_field.field_type = fitz.PDF_WIDGET_TYPE_CHECKBOX
    sub_field.rect = fitz.Rect(300, 250, 320, 270)
    sub_field.field_value = "Off"
    sub_field.button_pressed = False
    page.add_widget(sub_field)

    # Checkbox: Terms
    terms_field = fitz.Widget()
    terms_field.field_name = "AgreeTerms"
    terms_field.field_type = fitz.PDF_WIDGET_TYPE_CHECKBOX
    terms_field.rect = fitz.Rect(300, 290, 320, 310)
    terms_field.field_value = "Off"
    terms_field.button_pressed = False
    page.add_widget(terms_field)

    # Save
    doc.save(output_path, garbage=4, deflate=True, clean=True)
    doc.close()

    # Verify
    with open(output_path, "rb") as f:
        data = f.read()
        sha256 = hashlib.sha256(data).hexdigest()
        print(f"Generated {output_path}")
        print(f"SHA256: {sha256}")
        print(f"Size: {len(data)} bytes")

    return sha256

if __name__ == "__main__":
    import sys
    import os

    output_dir = os.path.join(os.path.dirname(__file__))
    os.makedirs(output_dir, exist_ok=True)
    output_path = os.path.join(output_dir, "field.pdf")

    sha256 = generate_field_pdf(output_path)

    # Write SHA256 to a sidecar file for verification
    with open(output_path + ".sha256", "w") as f:
        f.write(sha256 + "  field.pdf\n")

    print("Done.")
