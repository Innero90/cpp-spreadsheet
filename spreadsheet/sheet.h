#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <map>
#include <set>

class Sheet : public SheetInterface
{
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    // Очищает unique_ptr вместе с ресурсом (ячейка с содержимым)
    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    // Группа методов для работы с зависимыми ячейками (теми, которые 
    // зависят от заданной).
    // Вынесены в Sheet, т.к.:
    // 1) Они сильно перегружают классы Cell
    // 2) Данные о зависимых ячейках не относятся к Cell, это "внешние" данные
    // 3) Можно обойтись единым словарем зависимых ячеек

    // Производит сброс кэша для указанной ячейки и всех зависящих от нее
    void InvalidateCell(const Position& pos);
    // Добавляет взаимосвязь "основная ячейка" - "зависящая ячейка".
    // dependent_cell чаще всего == this
    void AddDependentCell(const Position& main_cell, const Position& dependent_cell);
    // Возвращает перечень ячеек, зависящих от pos
    const std::set<Position> GetDependentCells(const Position& pos);
    // Удаляет все зависимости для ячейки pos.
    void DeleteDependencies(const Position& pos);

private:
    // Единый для всего листа словарь зависимых ячеек (ячейка - список зависимых от нее)
    std::map<Position, std::set<Position>> cells_dependencies_;

    std::vector<std::vector<std::unique_ptr<Cell>>> sheet_;
    /* Мой комментарий к вектору векторов для ревью (все равно вопросы возникнут)
    *  1) Такой вариант валиден, предусмотрен подсказкой
    *  2) Укладывается в заданные временнЫе рамки требований к скорости алгоритмов
    *  3) Более легкая организация вектора векторов компенсируется более сложной
    *     частью навигации по индексам при модификациях листа.
    *  4) На рынке присутствует коммерческий пакет open office, который ставит на колени
    *     компьютеры с гигабайтами оперативной памяти при загрузке эксельной таблицы 
    *     на несколько десятков мегабайт. Очевидно, организация памяти в Excel оптимизирована, 
    *     а в open offece - нет, прямой аналог вектора векторов. Что, еще раз, 
    *     не мешает проекту быть коммерчески успешным.
    */

    int max_row_ = 0;    // Число строк в Printable Area
    int max_col_ = 0;    // Число столбцов в Printable Area
    bool area_is_valid_ = true;    // Флаг валидности текущих значений max_row_/col_

    // Пересчитывет максимальный размер области печати листа
    void UpdatePrintableSize();
    // Проверяет существование ячейки по координатам pos, сама она может быть nullptr
    bool CellExists(Position pos) const;
    // Резервирует память для ячейки, если ячейка с таким адресом не существует.
    // Не производит непосредственное создание элементов и не инвалидирует Printale Area
    void Touch(Position pos);
};
