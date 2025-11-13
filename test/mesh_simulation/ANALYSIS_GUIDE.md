# Mesh Network Analysis Guide

Ръководство за анализ на резултатите от mesh network симулациите.

## Бързо Стартиране

### 1. Стандартни Тестове (1-2 минути)
```batch
REM Baseline - нормална работа
python main.py --scenario 1 --nodes 15 --duration 60

REM High Traffic - натоварване
python main.py --scenario 2 --nodes 15 --duration 30

REM Root Failure - отказ на root
python main.py --scenario 3 --nodes 15 --duration 60
```

### 2. Stress Test (10-30 минути)
```batch
REM Малък stress test
python scenario_stress_test.py --nodes 50 --active 20 --duration 10

REM Среден stress test
python scenario_stress_test.py --nodes 200 --active 80 --duration 30

REM Пълен stress test (ТЕЖЪК!)
python scenario_stress_test.py --nodes 500 --active 200 --duration 60
```

## Ключови Метрики

### 1. Packet Loss Rate
**Какво показва:** Процент изгубени съобщения

**Интерпретация:**
- **< 0.1%**: Отлична производителност
- **0.1% - 1%**: Добра производителност
- **1% - 5%**: Приемливо, но има проблеми
- **5% - 10%**: Значителни проблеми
- **> 10%**: Критично състояние

**Причини за високи стойности:**
- Претоварени nodes (queue overflow)
- Твърде висок message rate
- Bottlenecks в tree topology
- Недостатъчен processing capacity

**Решения:**
- Намалете message rate
- Увеличете queue size
- Балансирайте tree topology
- Добавете buffering

### 2. End-to-End Latency
**Какво показва:** Време от source до destination

**Интерпретация:**
- **< 50ms**: Отлично за real-time apps
- **50-100ms**: Добро за повечето приложения
- **100-500ms**: Приемливо за non-real-time
- **> 500ms**: Проблематично

**Влияещи фактори:**
- Tree depth (hops)
- Node processing delay
- Queue wait time
- Network congestion

**P95 Latency:**
- 95% от съобщенията са по-бързи от тази стойност
- По-важна от средната latency
- Показва worst-case performance

### 3. Node Load
**Какво показва:** Натоварване на отделни nodes

**Метрики per node:**
- **Messages Sent**: Колко съобщения създава
- **Messages Received**: Колко съобщения получава
- **Messages Forwarded**: Колко съобщения препраща
- **Messages Dropped**: Колко съобщения губи

**Load Indicator:**
```
Load = Sent × 1.0 + Received × 1.0 + Forwarded × 2.0
```
Forwarding е по-скъпо от sending/receiving.

**Проблемни patterns:**
- Root node с много forwarding
- Intermediate nodes като bottlenecks
- Неравномерно разпределение

### 4. Message Queue Length
**Какво показва:** Колко съобщения чакат за processing

**Интерпретация:**
- **Avg Queue < 5**: Добро
- **Avg Queue 5-20**: Натоварено
- **Avg Queue > 20**: Критично
- **Max Queue = queue_size**: Overflow!

**Warning signs:**
- Растяща average queue
- Често достигане до max
- Голяма разлика между avg и max

### 5. Network Throughput
**Какво показва:** Обща data-carrying capacity

**Формула:**
```
Throughput = (Total Payload Delivered) / (Simulation Time)
```

**Използване:**
- Определяне на max capacity
- Comparison между topologies
- Scalability analysis

## Анализ по Сценарии

### Scenario 1: Baseline Performance

**Цел:** Установяване на baseline метрики

**Очаквани резултати:**
- Packet Loss: < 0.5%
- Latency: 10-50ms
- Balanced node load
- Low queue lengths

**Red flags:**
- Packet loss > 1%
- Latency > 100ms
- Unbalanced load
- Growing queues

**Действия:**
- Документирайте baseline
- Сравнете с други scenarios
- Identify optimal configuration

### Scenario 2: High Traffic

**Цел:** Тест на capacity при висок трафик

**Очаквани резултати:**
- Packet Loss: 1-5%
- Latency: 50-200ms
- High forwarding on root
- Higher queue lengths

**Red flags:**
- Packet loss > 10%
- Latency > 500ms
- Queue overflow
- Node failures

**Insights:**
- Max sustainable traffic rate
- Bottleneck identification
- Failure points

### Scenario 3: Root Node Failure

**Цел:** Network resilience и recovery

**Ключови метрики:**
- **Network Down Time**: Време до new root election
- **Packet Loss during transition**
- **Recovery latency**

**Добри показатели:**
- Recovery < 5s
- Packet loss < 20% during transition
- Successful reformation

**Проблеми:**
- Slow recovery > 10s
- High packet loss > 50%
- Failed reformation

### Stress Test: 500 Nodes

**Цел:** Extreme scale testing

**Focus Areas:**
1. **Tree Topology**
   - Actual depth vs max depth
   - Children distribution
   - Balance quality

2. **Load Distribution**
   - Hot spots identification
   - Root node performance
   - Level-by-level analysis

3. **Scalability Limits**
   - Max nodes at 2 msg/s
   - Max message rate at N nodes
   - Breaking points

## Tree Topology Analysis

### Depth vs Performance

**Shallow Tree (depth 3-5):**
- ✅ Low latency
- ✅ Fast message delivery
- ❌ High load on top nodes
- ❌ Many children per node

**Medium Tree (depth 6-10):**
- ✅ Balanced load
- ✅ Moderate children per node
- ⚠️ Moderate latency
- ⚠️ Some hot spots

**Deep Tree (depth > 10):**
- ❌ High latency
- ❌ Many hops
- ✅ Low load per node
- ✅ Few children per node

### Optimal Configuration

**За 500 nodes:**
```
Optimal Depth: 7-9 hops
Children per Node: 3-5
Root Children: 4-6
Message Rate: 1-2 msg/s
```

**Reasoning:**
- Depth 7-9: Balance между latency и load
- 3-5 children: Manageable forwarding load
- Root не е overwhelmed
- Sustainable message rates

## Load Distribution Patterns

### Pattern 1: Root Bottleneck
```
Depth 0 (Root):    Fwd=12000, Recv=24000  ❌ BOTTLENECK
Depth 1:           Fwd=2400,  Recv=22000
Depth 2:           Fwd=480,   Recv=20000
```

**Problem:** Root forwards all messages
**Solution:** Limit root children, use multiple roots

### Pattern 2: Balanced Load
```
Depth 0 (Root):    Fwd=3000,  Recv=6000   ✅ GOOD
Depth 1:           Fwd=2800,  Recv=5800
Depth 2:           Fwd=2600,  Recv=5600
```

**Good:** Gradual decrease
**Indicates:** Well-balanced tree

### Pattern 3: Mid-Level Hotspot
```
Depth 0 (Root):    Fwd=2000,  Recv=5000
Depth 3:           Fwd=8000,  Recv=12000  ❌ HOTSPOT
Depth 4:           Fwd=1500,  Recv=4000
```

**Problem:** Bridge node overloaded
**Solution:** Restructure subtree

## Препоръки за EspHub

### За Production Deployment

**Network Size:**
- **Препоръчително:** 50-100 nodes
- **Max с добра производителност:** 200 nodes
- **Теоретичен max:** 500 nodes при 1 msg/s

**Tree Configuration:**
- **Max Depth:** 8 hops
- **Children per Node:** 3-4
- **Message Rate:** 1-2 msg/s per node

**Queue Settings:**
- **Queue Size:** 50-100 messages
- **Processing Delay:** 5-10ms
- **Timeout:** 30s

### Monitoring & Alerts

**Critical Alerts:**
- Packet Loss > 5%
- Latency > 500ms
- Queue Length > 80% capacity
- Root node forwarding > 50% of traffic

**Warning Alerts:**
- Packet Loss > 1%
- Latency > 200ms
- Queue Length > 50% capacity
- Unbalanced children distribution

### Optimization Strategies

**1. Message Rate Control**
```cpp
// Adaptive rate based on queue length
if (queueLength > threshold) {
    messageInterval *= 1.5;  // Slow down
} else {
    messageInterval *= 0.9;  // Speed up
}
```

**2. Smart Routing**
```cpp
// Prefer less loaded neighbors
Node* selectBestParent() {
    return min_element(neighbors,
        [](Node* a, Node* b) {
            return a->load < b->load;
        });
}
```

**3. Queue Management**
```cpp
// Priority queue with message types
PriorityQueue<Message> queue;
queue.push(msg, priority);
```

## Reporting Template

### Executive Summary
```
Network Configuration: 500 nodes, max 10 hops
Test Duration: 60 seconds
Message Rate: 400 msg/s (200 active × 2 msg/s)

Key Results:
- Packet Loss: 0.05% ✅
- Average Latency: 23ms ✅
- P95 Latency: 45ms ✅
- Max Queue Length: 15 ✅

Conclusion: Network performs well under stress.
Recommended max: 400 nodes at 2 msg/s.
```

### Detailed Analysis
```
Load Distribution:
- Root Node: Forwarded 12,000 msgs (balanced)
- Level 1-3: Moderate load
- Level 4-8: Low load
- Hot spots: Nodes 23, 47 (investigate)

Bottlenecks: None identified
Failure Points: None

Recommendations:
1. Current configuration is optimal
2. Can scale to 600 nodes at 1.5 msg/s
3. Monitor nodes 23, 47 for imbalance
```

## Troubleshooting Guide

### Symptom: High Packet Loss

**Checklist:**
1. Check queue sizes (overflow?)
2. Review message rate (too high?)
3. Analyze node load distribution
4. Check tree depth (too deep?)

**Actions:**
- Увеличете queue size
- Намалете message rate
- Rebalance tree
- Add more children to root

### Symptom: High Latency

**Checklist:**
1. Measure tree depth
2. Check queue wait times
3. Review processing delays
4. Analyze congestion points

**Actions:**
- Reduce tree depth
- Optimize processing
- Clear congestion
- Add bypass routes

### Symptom: Unbalanced Load

**Checklist:**
1. Review children distribution
2. Check root node load
3. Identify hot spots
4. Analyze message patterns

**Actions:**
- Redistribute children
- Limit root connections
- Restructure subtrees
- Load balancing algorithm

## Next Steps

1. **Run all scenarios** для comprehensive analysis
2. **Compare results** across configurations
3. **Identify optimal settings** for EspHub
4. **Document findings** in final report
5. **Implement improvements** in actual code

---

**Last Updated:** 2025-11-13
**Version:** 1.0
**Purpose:** Analysis guide for mesh simulation results
