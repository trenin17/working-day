import fitz
def get_stamp_text(stamp_data):
    """Генерирует текст для штампа подписи."""
    stamp_text = """
    Документ подписан электронной подписью в системе "Рабочий День"
    
    Подписант: {name}
    Реквизиты проверки ключа ЭП: {user_id}
    Идентификатор документа: {document_id}
    Тип подписи, дата и время подписания: {sign_type}, {date}
    Организация: {organization}
    """.format(
        date=stamp_data.date,
        name=stamp_data.name,
        organization=stamp_data.organization,
        user_id=stamp_data.user_id,
        document_id=stamp_data.document_id,
        sign_type=stamp_data.sign_type,
    )
    return stamp_text

def get_stamp_borders(page_width, page_height, stamp_height=200, stamp_width=401):
    """Возвращает границы штампа подписи (центр внизу страницы)."""
    x1 = (page_width - stamp_width) / 2
    y1 = page_height - stamp_height - 20  # 20 - отступ снизу
    x2 = x1 + stamp_width
    y2 = y1 + stamp_height
    return (x1, y1, x2, y2)

def get_doc_id_borders(page_width, page_height, box_height=17, box_width=220):
    """Возвращает границы прямоугольника с ID документа (нижний колонтитул)."""
    x1 = page_width - box_width - 20  # 20 - отступ справа
    y1 = page_height - box_height - 10  # 10 - отступ снизу
    x2 = x1 + box_width
    y2 = y1 + box_height
    return (x1, y1, x2, y2)

def add_doc_id_to_page(page, document_id):
    """Добавляет ID документа в нижний колонтитул страницы."""
    page_width, page_height = page.rect.width, page.rect.height
    doc_id_rect = fitz.Rect(get_doc_id_borders(page_width, page_height))
    doc_id_color = (0, 0.5, 1)  # Синий цвет
    
    # Рисуем прямоугольник
    page.draw_rect(doc_id_rect, color=doc_id_color, width=1)
    
    # Вставляем текст ID
    font = fitz.Font("tiro")
    page.insert_font(fontname="F0", fontbuffer=font.buffer)
    page.insert_textbox(
        doc_id_rect,
        f"doc_ID: {document_id}",
        fontsize=10,
        fontname="F0",
        color=doc_id_color,
        align=1,  # Выравнивание по левому краю
    )

def add_signature_stamp(page, stamp_data):
    """Добавляет штамп подписи на страницу."""
    page_width, page_height = page.rect.width, page.rect.height
    stamp_rect = fitz.Rect(get_stamp_borders(page_width, page_height))
    stamp_color = (0, 0.5, 1)  # Синий цвет
    
    # Рисуем прямоугольник штампа
    page.draw_rect(stamp_rect, color=stamp_color, width=1)
    
    # Вставляем текст штампа
    font = fitz.Font("tiro")
    page.insert_font(fontname="F0", fontbuffer=font.buffer)
    stamp_text = get_stamp_text(stamp_data)
    page.insert_textbox(
        stamp_rect,
        stamp_text,
        fontsize=12,
        fontname="F0",
        color=stamp_color,
        align=0,  # Выравнивание по левому краю
    )

def create_stamp(path_to_pdf, path_to_new_pdf, stamp_data, is_first_signature=True):
    """
    Добавляет подпись в PDF:
    - На все страницы, кроме последней, добавляет ID документа.
    - На последнюю страницу добавляет штамп подписи.
    - Если is_first_signature=False, добавляет новую страницу для подписи.
    """
    doc = fitz.open(path_to_pdf)
    if is_first_signature:
        # Добавляем ID документа на все страницы, кроме последней
        for i in range(len(doc) - 1):
            page = doc.load_page(i)
            add_doc_id_to_page(page, stamp_data.document_id)
    else:
        last_page = doc.load_page(len(doc) - 1)
        new_page = doc.new_page(width=last_page.rect.width, height=last_page.rect.height)
    
    # Добавляем штамп подписи на последнюю страницу
    last_page = doc.load_page(len(doc) - 1)
    add_signature_stamp(last_page, stamp_data)
    
    # Сохраняем изменения
    doc.save(path_to_new_pdf)
    doc.close()
    return "200"

class StampData:
    def __init__(self, date, name, organization, user_id, document_id, sign_type="Простая ЭП"):
        self.date = date
        self.name = name
        self.organization = organization
        self.user_id = user_id
        self.document_id = document_id
        self.sign_type = sign_type