# Quick Start Guide - Mesh Network Simulation

Бързо ръководство за стартиране на mesh network симулацията.

## Стъпка 1: Инсталация

```batch
REM Стартирайте setup скрипта
setup.bat

REM Или ръчно:
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

## Стъпка 2: Пускане на първа симулация

```batch
REM Активирайте виртуалната среда
venv\Scripts\activate

REM Стартирайте Scenario 1 (бързо - 10 секунди)
python main.py --scenario 1 --nodes 10 --duration 10
```

## Стъпка 3: Преглед на резултатите

Резултатите се записват в `results/` директория:

```batch
REM Markdown доклад
type results\Scenario_1_Baseline_report.md

REM JSON данни
type results\Scenario_1_Baseline_XXXXXXXX_XXXXXX.json

REM Графики
start results\Scenario_1_Baseline_load.png
```

## Стъпка 4: Стартиране на всички сценарии

```batch
REM Стартиране на всички 7 сценария
python main.py --all
```

**Внимание:** Това може да отнеме 15-30 минути!

## Стъпка 5: Генериране на визуализации

```batch
REM Генериране на всички графики за анализ
python visualization.py --all
```

## Какво означават резултатите?

### Добра производителност
- **Packet Loss < 1%** - Няма загуба на пакети
- **Latency < 100ms** - Бърза доставка
- **Balanced Node Load** - Равномерно разпределение

### Внимание
- **Packet Loss 1-10%** - Умерена загуба
- **Latency 100-1000ms** - Забавяне
- **High Queue Lengths** - Претоварени nodes

### Проблеми
- **Packet Loss > 10%** - Критична загуба
- **Latency > 1000ms** - Много бавно
- **Growing Queues** - Nodes не се справят

## Полезни команди

```batch
REM Бърз тест с 5 nodes, 30 секунди
python main.py --scenario 1 --nodes 5 --duration 30

REM Тест на Root Node failure
python main.py --scenario 3

REM Scalability тест до 50 nodes
python main.py --scenario 5 --nodes 50

REM Генериране на comparison график
python visualization.py --compare

REM Генериране на scalability анализ
python visualization.py --scalability
```

## Често срещани проблеми

### "Module not found"
```batch
REM Уверете се че виртуалната среда е активирана
venv\Scripts\activate
```

### "No results found"
```batch
REM Първо стартирайте симулация
python main.py --scenario 1 --nodes 10 --duration 10

REM След това визуализации
python visualization.py --all
```

### Бавно изпълнение
```batch
REM Използвайте по-малко nodes
python main.py --scenario 1 --nodes 5

REM Или по-кратка продължителност
python main.py --scenario 2 --duration 30
```

## Следващи стъпки

1. **Прегледайте [README.md](README.md)** за пълна документация
2. **Проучете [SCENARIOS.md](SCENARIOS.md)** за детайли за сценариите
3. **Вижте [METRICS.md](METRICS.md)** за обяснение на метриките
4. **Експериментирайте** с различни параметри
5. **Анализирайте** резултатите и направете препоръки

## Пример работен поток

```batch
REM 1. Setup
setup.bat

REM 2. Активирай среда
venv\Scripts\activate

REM 3. Бърз тест
python main.py --scenario 1 --nodes 10 --duration 10

REM 4. Преглед на резултати
type results\Scenario_1_Baseline_report.md

REM 5. Пълен run на всички сценарии
python main.py --all

REM 6. Генериране на графики
python visualization.py --all

REM 7. Преглед на резултати
dir results\
```

## Допълнителна помощ

- Прочетете пълната документация в [README.md](README.md)
- Проверете log файловете за грешки
- Използвайте `--help` за опции: `python main.py --help`

---

**Готови! Започнете с `python main.py --scenario 1 --nodes 10 --duration 10`**
