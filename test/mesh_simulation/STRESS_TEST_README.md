# Stress Test - Large Scale Mesh Network Analysis

## Описание

Специализиран сценарий за тестване на mesh мрежата при екстремни условия:
- **500 nodes** в мрежата
- **200 активни изпращачи**
- **2 съобщения/секунда** на node
- **Max 10 hops** дълбочина на tree
- **400 msg/s** общ network трафик

## Файлове

- **scenario_stress_test.py** - Stress test implementation
- **results/StressTest_*.json** - Резултати в JSON формат
- **results/StressTest_*_report.md** - Подробен доклад
- **results/StressTest_*_load.png** - Node load визуализация

## Използване

### Бърз тест (50 nodes, 10s)
```batch
cd test\mesh_simulation
venv\Scripts\activate
python scenario_stress_test.py --nodes 50 --active 20 --depth 10 --rate 2 --duration 10
```

### Среден тест (200 nodes, 30s)
```batch
python scenario_stress_test.py --nodes 200 --active 80 --depth 10 --rate 2 --duration 30
```

### Пълен stress test (500 nodes, 60s)
```batch
python scenario_stress_test.py --nodes 500 --active 200 --depth 10 --rate 2 --duration 60
```

**ВНИМАНИЕ:** Пълният тест може да отнеме **10-30 минути** за изпълнение!

### Параметри

```
--nodes NODES       Total number of nodes (default: 500)
--active ACTIVE     Number of active senders (default: 200)
--depth DEPTH       Max tree depth in hops (default: 10)
--rate RATE         Messages per second per node (default: 2)
--duration DURATION Simulation duration in seconds (default: 60)
```

## Какво се тества?

### 1. Tree Topology with Depth Constraint
- Balanced tree структура
- Контролирана дълбочина (max 10 hops)
- Разпределение на nodes по levels
- Children per node statistics

### 2. Network Load
- 200 nodes изпращат съобщения активно
- 2 съобщения/секунда = 400 msg/s network load
- Total traffic: ~24,000 съобщения за 60 секунди
- ~12 милиона deliveries (500 nodes × 24,000 messages)

### 3. Node Performance
- Messages forwarded по node
- Queue lengths
- Packet loss rate
- Processing delays

### 4. Load Distribution
- Load по tree depth
- Most loaded nodes analysis
- Bottleneck identification
- Hot spots в мрежата

## Output & Резултати

### Console Output

#### Setup Phase
```
Tree Statistics:
  Actual Tree Depth: 8 hops
  Average Children per Node: 4.23
  Max Children per Node: 10
  Root Node Children: 5

  Node Distribution by Depth:
    Depth  0:    1 nodes (  0.2%)
    Depth  1:    5 nodes (  1.0%)
    Depth  2:   25 nodes (  5.0%)
    ...
```

#### Runtime Statistics
```
Top 10 Most Loaded Nodes:
  Node   Root  Depth Children     Sent     Recv      Fwd  Dropped   MaxQ
  1      Yes      0        5        0    24000    12000        0     15
  2      No       1        5     6000    22000    11000        0     12
  ...
```

#### Load by Depth
```
Load Distribution by Tree Depth:
 Depth  Nodes    Avg Fwd   Avg Recv  Avg Dropped
     0      1    12000.0   24000.0          0.0
     1      5    11000.0   22000.0          0.0
     2     25     8500.0   20000.0          5.2
  ...
```

### Report Files

#### JSON Results (results/StressTest_*.json)
```json
{
  "scenario": "StressTest_500nodes_200active",
  "statistics": {
    "total_nodes": 500,
    "delivered_messages": 12000000,
    "packet_loss_rate": 0.05,
    "latency": {
      "avg": 0.023,
      "p95": 0.045
    }
  }
}
```

#### Markdown Report (results/StressTest_*_report.md)
- Network overview
- Message statistics
- Per-node statistics table
- Load analysis
- Recommendations

### Visualizations

#### Node Load Chart
Bar chart показващ forwarded messages per node (top nodes).

## Анализ на резултатите

### Критични метрики

1. **Packet Loss Rate**
   - < 1%: Отлично
   - 1-5%: Приемливо
   - \> 5%: Проблем

2. **Average Latency**
   - < 50ms: Отлично
   - 50-200ms: Добро
   - \> 200ms: Бавно

3. **Root Node Load**
   - Forwarded messages
   - Queue length
   - Bottleneck risk

4. **Load Distribution**
   - Балансирано ли е натоварването?
   - Hot spots по depth levels
   - Leaf nodes vs intermediate nodes

### Интерпретация

#### Добра производителност
```
Packet Loss: 0.05%
Avg Latency: 23ms
Max Queue: 15 messages
Root Load: Balanced with children
```

#### Проблеми
```
Packet Loss: 8.5%
Avg Latency: 450ms
Max Queue: 85 messages
Root Load: Heavily overloaded
```

## Препоръки

### За Оптимизация

1. **Намаляване на Tree Depth**
   - По-малка дълбочина = по-малко hops
   - По-добра latency
   - По-малко packet loss

2. **Load Balancing**
   - Разпределяне на children равномерно
   - Избягване на hot spots
   - Multi-root topology

3. **Queue Management**
   - Увеличаване на queue size
   - Priority queuing
   - Flow control

4. **Message Rate Control**
   - Rate limiting per node
   - Adaptive rate based on load
   - Congestion control

### За Scalability

**Мрежата работи добре до:**
- ~300 nodes при 2 msg/s
- ~500 nodes при 1 msg/s
- ~200 nodes при 5 msg/s

**При повече nodes:**
- Намалете message rate
- Увеличете queue size
- Разгледайте multi-mesh architecture

## Performance Tips

### За по-бързо изпълнение

1. **Намалете продължителността:**
   ```batch
   python scenario_stress_test.py --duration 10
   ```

2. **По-малко nodes:**
   ```batch
   python scenario_stress_test.py --nodes 200
   ```

3. **По-малко активни изпращачи:**
   ```batch
   python scenario_stress_test.py --active 50
   ```

### За по-реалистичен тест

1. **Variable message rate:**
   - Модифицирайте кода за random intervals
   - Burst traffic patterns
   - Periodic peaks

2. **Node failures:**
   - Simulate node crashes
   - Network partitions
   - Recovery scenarios

## Известни ограничения

1. **Memory Usage**
   - 500 nodes × 24,000 messages = много памет
   - Може да се наложи swap
   - Затваряне на други приложения

2. **Execution Time**
   - 500 nodes може да отнеме 10-30 минути
   - Зависи от CPU скорост
   - Background режим препоръчителен

3. **Visualization**
   - Latency plots могат да са бавни
   - Load charts с 500 nodes са неясни
   - Използвайте JSON за детайлен анализ

## Примерен workflow

```batch
REM 1. Quick test
python scenario_stress_test.py --nodes 50 --duration 10

REM 2. Review results
type results\StressTest_50nodes_20active_report.md

REM 3. Medium test
python scenario_stress_test.py --nodes 200 --duration 30

REM 4. Full stress test (in background)
start /B python scenario_stress_test.py --nodes 500 --duration 60

REM 5. Monitor progress (wait for completion)
REM Check results\ directory for output files

REM 6. Analyze results
type results\StressTest_500nodes_200active_report.md
```

## Troubleshooting

### "Memory Error"
- Намалете броя nodes: `--nodes 200`
- Намалете duration: `--duration 30`
- Затворете други приложения

### "Slow execution"
- Нормално за 500 nodes
- Използвайте background mode
- Намалете active senders

### "No latency data"
- Deepcopy на broadcast messages не се track правилно
- Латенцията се измерва за оригиналните messages
- Broadcast deliveries са counted отделно

## Следващи стъпки

1. **Анализирайте резултатите** от 500 node test
2. **Идентифицирайте bottlenecks** в tree topology
3. **Експериментирайте** с различни tree depths
4. **Сравнете** performance при different configurations
5. **Документирайте findings** за EspHub

## Ресурси

- [Main README](README.md) - Пълна документация
- [SCENARIOS.md](SCENARIOS.md) - Други сценарии
- [METRICS.md](METRICS.md) - Метрики обяснения

---

**Created:** 2025-11-13
**Status:** OPERATIONAL
**Purpose:** Large-scale mesh network stress testing
