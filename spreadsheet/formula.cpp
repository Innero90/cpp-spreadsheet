#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>
#include <set>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe)
{
    // Выводит "#REF!", "#VALUE!", "#DIV/0!" или ""
    return output << fe.ToString();

    /* PREVIOUS VERSION
    // Пока обрабатываем только ошибку деления на ноль
    //return output << "#DIV/0!";
    */
}

/*
*  Классы Formula, FormulaInterface и все остальные классы 
*  более высого уровня передают ссылку на таблицу Sheet& для 
*  доступа к членам классов.
* 
*  Классы FormulaAST, Expr и все остальные классы более низкого 
*  уровня передают функтор std::function<double(Position)>& 
*  (здесь реализован в виде лямбда-функции), получающий адрес
*  ячейки в качестве аргумента и ссылку на таблицу в [], что 
*  позволяет производить расчеты значений ячеек по формулам.
*  Реально расчет при помощи функтора делает только CellExpr, 
*  т.к. только у него есть поле с указателем на индекс ячейки.
*/
namespace {
class Formula : public FormulaInterface {
public:
// Реализуйте следующие методы:
    explicit Formula(std::string expression) 
        : ast_(ParseFormulaAST(std::move(expression))), 
          referenced_cells_(ast_.GetCells().begin(), ast_.GetCells().end())
    {}

    Value Evaluate(const SheetInterface& sheet) const override
    {
        /*
        * FormulaAST ast_ ничего не знает о существовании таблицы. 
        * Но она хранит наименования позиций - индексы ячеек, которые
        * имеют смысл для таблицы. Функтор позволяет производить 
        * обращение к ячейкам таблицы через их индексы (Position)
        */
        try
        {
            return ast_.Execute(
                // Лямбда-функция в качестве функтора.
                // У нас CellExpr, в котором есть указатель на индекс ячейки pos.
                // Сама ячейка принадлежит листу таблицы sheet.
                // Т.к. мы в классе Formula (не Text! не Empty!) и это CellExpr,
                // то ни текстом, ни пустой строкой результат вычисления быть не может.
                [&sheet](const Position& pos)
                {
                    // 1. Если ячейка не существует, трактуем это как 0
                    if (sheet.GetCell(pos) == nullptr)
                    {
                        return 0.0;
                    }

                    auto value = sheet.GetCell(pos)->GetValue();

                    // 2. Если ячейка содержит число, возвращаем его как есть
                    if (std::holds_alternative<double>(value))
                    {
                        return std::get<double>(value);
                    }
                    // 3. Если ячейка содержит текст, пытаемся его интерпретировать
                    //    как число.
                    else if (std::holds_alternative<std::string>(value))
                    {
                        try
                        {
                            // string без move(get...), чтобы не разрушить значение из variant
                            // return std::stod(std::get<std::string>(value)); - без проверки пропускает 3D и прочий текст
                            std::string tmp = std::get<std::string>(value);
                            if (std::all_of(tmp.cbegin(), tmp.cend(), [](char ch)
                                            {
                                                return (std::isdigit(ch) || ch == '.');
                                            }))
                            {
                                return std::stod(tmp);
                            }
                            else
                            {
                                // Текст не может быть сконвертирован в число
                                throw FormulaError(FormulaError::Category::Value);

                                // Подавляем предупреждение компилятора об отсутствии возвращаемого значения
                                return 0.0;
                            }
                        }
                        catch (...)
                        {
                            // Текст не может быть сконвертирован в число
                            // (для случая выбрасывания исключения при конвертации)
                            throw FormulaError(FormulaError::Category::Value);
                        }
                    }
                    // 4. Иначе ячейка содержит CellInterface::FormulaError
                    else
                    {
                        throw std::get<FormulaError>(value);
                    }

                    // Заглушка для подавления предупреждений компилятора 
                    // об отсутствии возвращаемых значений для некоторых веток
                    return 0.0;
                }
            );
        }
        catch (FormulaError& ex_fe)
        {
            return ex_fe;
        }
    }

    // Используем "очищенную" формулу без лишних скобок из 
    // FormulaAST::PrintFormula(std::ostream& out).
    std::string GetExpression() const override
    {
        std::stringstream ss;
        ast_.PrintFormula(ss);
        return ss.str();
    }

    std::vector<Position> GetReferencedCells() const override
    {
        // Вектор результатов (будет возвращен через NVRO)
        std::vector<Position> result;

        // Убираем дублирование ячеек, прогоняя их через std::set
        std::set<Position> unique_ref_cells(referenced_cells_.begin(), referenced_cells_.end());
        for (const auto& cell : unique_ref_cells)
        {
            result.push_back(cell);
        }
        return result;  //NVRO
    }


private:
    FormulaAST ast_;
    std::vector<Position> referenced_cells_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    try
    {
        return std::make_unique<Formula>(std::move(expression));
    }
    catch (const std::exception&)
    {
        throw FormulaException("Formula parse error");
    }
}
