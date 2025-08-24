from aiohttp import web
import pandas as pd
import tempfile
from openpyxl import load_workbook
from openpyxl.styles import Font
from s3_client.aws_utils import upload_and_presign

REPLACEMENTS = {
    "work_daytime": "Я",                    # Дневная работа
    "work_nighttime": "Н",                  # Ночная работа
    "work_weekend_holiday": "РВ",           # Работа в выходные/праздники
    "overtime": "C",                        # Сверхурочные
    "shortened_work_time": "ЛЧ",            # Сокращенное рабочее время
    
    "vacation": "ОТ",                       # Отпуск
    "additional_paid_vacation": "ОД",       # Дополнительный оплачиваемый отпуск
    "unpaid_vacation": "ДО",                # Отпуск без содержания

    "study_vacation_paid": "У",             # Учебный отпуск (оплачиваемый)
    "study_vacation_unpaid": "УД",          # Учебный отпуск (неоплачиваемый)

    "maternity_leave": "Р",                 # Декретный отпуск
    "childcare_leave": "ОЖ",                # Отпуск по уходу за ребенком

    "sick_leave": "Б",                      # Больничный

    "business_trip": "K",                   # Командировка

    "absence_without_reason": "ПР",         # Прогул
    "absence_unknown_reason": "НН",         # Отсутствие по неизвестной причине

    "part_time_by_employer": "НС",          # Неполное время по инициативе работодателя
    "weekend_holiday": "В",                 # Выходной/праздник
    "downtime_not_employer_fault": "НП",    # Простой не по вине работодателя
    "suspension_with_pay": "НО",            # Отстранение с оплатой
}

WORKING_DAYS = (
    "work_daytime", 
    "work_nighttime",
    "work_weekend_holiday",
    "overtime",
    "shortened_work_time",
    "part_time_by_employer"
)     

def transform_attendance(attendances: list[dict], date_from: str, date_to: str) -> dict[str, pd.DataFrame]:
    df = pd.DataFrame(attendances)

    # ФИО + компания
    df["ФИО"] = df["employee"].apply(
        lambda e: f"{e.get('surname','')} {e.get('name','')} {e.get('patronymic','')}".strip()
    )
    df["Компания"] = df["employee"].apply(lambda e: e.get("subcompany", ""))

    df["start_date"] = pd.to_datetime(df["start_date"])
    df["end_date"] = pd.to_datetime(df["end_date"])

    # приоритет: abscence_type > attendance_type
    df["value"] = df.apply(
        lambda r:  r.get("abscence_type") or r.get("attendance_type") or "",
        axis=1
    )

    # подмена значений по словарю REPLACEMENTS
    df["value"] = df["value"].map(lambda v: REPLACEMENTS.get(v, v))

    # часы считаем только для рабочих смен (WORKING_DAYS)
    df["hours"] = df.apply(
        lambda r: ((r["end_date"] - r["start_date"]).total_seconds() / 3600.0)
        if r.get("attendance_type") in WORKING_DAYS else 0.0,
        axis=1
    )

    all_days = pd.date_range(date_from, date_to, freq="D")

    company_tables: dict[str, pd.DataFrame] = {}
    for company, group in df.groupby("Компания"):
        rows = []
        for _, r in group.iterrows():
            start_norm = r["start_date"].normalize()
            end_norm = (r["end_date"].normalize()
                        if pd.notna(r["end_date"]) else start_norm)
            for day in pd.date_range(start_norm, end_norm, freq="D"):
                rows.append({
                    "ФИО": r["ФИО"],
                    "day": day,
                    "value": r["value"],
                    "hours": r["hours"] if day == start_norm else 0.0,
                })

        df_days = pd.DataFrame(rows)

        pivot = df_days.pivot_table(
            index="ФИО",
            columns="day",
            values="value",
            aggfunc=lambda x: ",".join(str(v) for v in x if pd.notna(v) and v != "")
        ).fillna("")

        pivot = pivot.reindex(columns=all_days, fill_value="")

        day_labels = {d: str(d.day) for d in all_days}
        pivot.rename(columns=day_labels, inplace=True)

        pivot.reset_index(inplace=True)

        work_hours = df_days.groupby("ФИО")["hours"].sum()
        pivot["work_hours"] = pivot["ФИО"].map(work_hours).fillna(0.0).round(2)

        ordered_cols = ["ФИО"] + [str(d.day) for d in all_days] + ["work_hours"]
        pivot = pivot.reindex(columns=ordered_cols, fill_value="")

        company_tables[company] = pivot

    return company_tables


async def generate_attendance_excel(request):
    file_key = request.rel_url.query['file_key']
    data = await request.json()
    attendances = data.get("attendances", [])

    if not attendances:
        return web.Response(status=400, text="Invalid input data")

    df_tmp = pd.DataFrame(attendances)
    if "start_date" not in df_tmp.columns or "end_date" not in df_tmp.columns:
        return web.Response(status=400, text="from/to not provided and cannot be derived")

    df_tmp["start_date"] = pd.to_datetime(df_tmp["start_date"])
    df_tmp["end_date"] = pd.to_datetime(df_tmp["end_date"])

    date_from = df_tmp["start_date"].min().strftime("%Y-%m-%d")
    date_to   = df_tmp["end_date"].max().strftime("%Y-%m-%d")

    company_tables = transform_attendance(attendances, date_from, date_to)

    with tempfile.NamedTemporaryFile(suffix=".xlsx", delete=False) as tmp:
        output = tmp.name

    with pd.ExcelWriter(output, engine="openpyxl") as writer:
        row_offset = 0
        for company, table in company_tables.items():
            pd.DataFrame([[f"Компания {company}"]]).to_excel(
                writer, sheet_name="Attendance", startrow=row_offset, index=False, header=False
            )
            row_offset += 1

            table.to_excel(
                writer, sheet_name="Attendance", startrow=row_offset, index=False
            )
            row_offset += len(table) + 2

    wb = load_workbook(output)
    ws = wb["Attendance"]
    for row in ws.iter_rows(min_row=1, max_row=ws.max_row):
        cell = row[0]
        if cell.value and str(cell.value).startswith("Компания "):
            cell.font = Font(bold=True, size=12)
    wb.save(output)

    url = upload_and_presign(output, file_key + ".xlsx")
    return web.Response(status=200, content_type="text/plain", text=url)
