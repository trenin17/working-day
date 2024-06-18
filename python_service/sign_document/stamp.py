import fitz

def get_stamp_text(stamp_data):
    #из даты получаем текст для штампа
    
    stamp_text = """
    Документ подписан электронной подписью в системе "Рабочий День"
    
    Подписант: {name}
    Реквизиты проверки ключа ЭП: {user_id}
    Идентификатор документа: {document_id}
    Тип подписи, дата и время подписания: {sign_type}, {date}
    Организация: {organization}
    """.format(date=stamp_data.date, 
               name=stamp_data.name, 
               organization=stamp_data.organization,
               user_id=stamp_data.user_id,
               document_id=stamp_data.document_id,
               sign_type=stamp_data.sign_type)
    return(stamp_text)

def get_stamp_borders(page_width, page_height, stamp_height = 200, stamp_width = 401):
    #возвращает границы штампа
    #это верхняя лев и нижняя прав границы штампа
    x1 = (page_width - stamp_width)/2
    y1 = page_height - stamp_height - 20 #20 - отступ снизу
    x2 = x1 + stamp_width
    y2 = y1 + stamp_height
    return(x1, y1, x2, y2)


def create_stamp(path_to_pdf, path_to_new_pdf, stamp_data):
    
    doc = fitz.open(path_to_pdf)
    
    page_number = 0
    page = doc.load_page(page_number)

    # Получить размеры страницы

    page_width, page_height = page.rect.width, page.rect.height
    stamp_position = fitz.Rect(get_stamp_borders(page_width, page_height))
    stamp_color = (0, 0.5, 1)
    rect_width = 1  # Ширина границы
    #рисуем штамп
    page.draw_rect(stamp_position, color=stamp_color, width=rect_width)
    
    font=fitz.Font("tiro")
    page.insert_font(fontname="F0", fontbuffer=font.buffer)
    stamp_text = get_stamp_text(stamp_data)
    page.insert_textbox(stamp_position, stamp_text, fontsize=12, 
                        fontname="F0", color=stamp_color,
                        align=0)

    # Сохранить изменения в новый файл
    doc.save(path_to_new_pdf)

    # Закрыть документ
    doc.close()
    
    return("200")


class StampData:    
    def __init__(self, date, name,
                 organization, user_id, document_id, sign_type = "Простая ЭП"):
            self.date = date
            self.name = name
            self.organization = organization
            self.user_id = user_id
            self.document_id = document_id
            self.sign_type = sign_type
