# Завендоренные зависимости (vendor)

Пакеты установлены локально через `pip3 install --target vendor` — глобально
не ставятся. Тексты лицензий лежат в `vendor/<packagename>.dist-info/licenses/`
и в комплект входят как есть, удалять их нельзя.

| Пакет  | Версия | Лицензия    | Источник                             |
|--------|--------|-------------|--------------------------------------|
| ddgs   | 9.16.0 | MIT         | https://github.com/deedy5/ddgs       |
| primp  | 2.0.0  | MIT         | https://github.com/deedy5/primp      |
| lxml   | 6.1.3  | BSD-3-Clause| https://github.com/lxml/lxml         |
| click  | 8.5.0  | BSD-3-Clause| https://click.palletsprojects.com/   |

Обновление:

    pip3 install --target src/ddg/vendor --upgrade ddgs