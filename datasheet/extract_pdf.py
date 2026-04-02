import pdfplumber

pdf = pdfplumber.open(r'd:\programing workspace\stm32vscode\ht1621\datasheet\ht1621b.PDF')
for i, p in enumerate(pdf.pages):
    text = p.extract_text()
    print(f'=== PAGE {i+1} ===')
    print(text)
    print()
pdf.close()
