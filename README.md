## SkipList
https://github.com/Shy2593666979/Redis-SkipList

## HyperLogLog
https://zhuanlan.zhihu.com/p/58519480



### HLL 算法原理

HyperLogLog 算法来源于论文 [*HyperLogLog the analysis of a near-optimal cardinality estimation algorithm*](http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf)，想要了解 HLL 的原理，先要从伯努利试验说起，伯努利实验说的是抛硬币的事。一次伯努利实验相当于抛硬币，不管抛多少次只要出现一个正面，就称为一次伯努利实验。

我们用 k 来表示每次抛硬币的次数，n 表示第几次抛的硬币，用 k_max 来表示抛硬币的最高次数，最终根据估算发现 n 和 k_max 存在的关系是 n=2^(k_max)，但同时我们也发现了另一个问题当试验次数很小的时候，这种估算方法的误差会很大，例如我们进行以下 3 次实验：

- 第 1 次试验：抛 3 次出现正面，此时 k=3，n=1；
- 第 2 次试验：抛 2 次出现正面，此时 k=2，n=2；
- 第 3 次试验：抛 6 次出现正面，此时 k=6，n=3。

对于这三组实验来说，k_max=6，n=3，但放入估算公式明显 3≠2^6。为了解决这个问题 HLL 引入了分桶算法和调和平均数来使这个算法更接近真实情况。

分桶算法是指把原来的数据平均分为 m 份，在每段中求平均数在乘以 m，以此来消减因偶然性带来的误差，提高预估的准确性，简单来说就是把一份数据分为多份，把一轮计算，分为多轮计算。

而调和平均数指的是使用平均数的优化算法，而非直接使用平均数。

> 例如小明的月工资是 1000 元，而小王的月工资是 100000 元，如果直接取平均数，那小明的平均工资就变成了 (1000+100000)/2=50500‬ 元，这显然是不准确的，而使用调和平均数算法计算的结果是 2/(1⁄1000+1⁄100000)≈1998 元，显然此算法更符合实际平均数。

所以综合以上情况，在 Redis 中使用 HLL 插入数据，相当于把存储的值经过 hash 之后，再将 hash 值转换为二进制，存入到不同的桶中，这样就可以用很小的空间存储很多的数据，统计时再去相应的位置进行对比很快就能得出结论，这就是 HLL 算法的基本原理，想要更深入的了解算法及其推理过程，可以看去原版的论文，链接地址在文末。

### 小结

当需要做大量数据统计时，普通的集合类型已经不能满足我们的需求了，这个时候我们可以借助 Redis 2.8.9 中提供的 HyperLogLog 来统计，它的优点是只需要使用 12k 的空间就能统计 2^64 的数据，但它的缺点是存在 0.81% 的误差，HyperLogLog 提供了三个操作方法 pfadd 添加元素、pfcount 统计元素和 pfmerge 合并元素。



## listpack

Redis 源码对于listpack的解释为“A list of strings serialization format”，一个字符串列表的序列格式化，也就是将一个字符串进行序列化存储。Redis listpack 可以用来存储整型或则字符串，结构如下

```cpp
// <Total Bytes><Num Elem><Entry1>...<EntryX><End>
// <Entry> --> <Encode><content><backlen>
```

1. Total Bytes 为整个listpack的空间大小，占用4个字节
2. Num Elem为listpack中元素的个数，即Entry的个数，占用两个字节，这并不意味着listpack最多只能存放65535个Entry，当Entry的个数超过65534时，Num Elem只保存65535，但是实际个数需要遍历来进行统计
3. End为结束标志，占用一个字节内容为0xFF，从代码看End其实应该两个字节，后面还有一个End自己的backlen？
4. Entry为listpack中具体的元素，Encode是该元素的编码方式占用一个字节，content是内容字段，backlen是前两者的总字节数，但不包含自身的字节数，一个backlen最多占用5个字节
5. 需要注意的是，整型存储中并不实际存储负数，而是将负数转换为正数进行存储，例如13位整型存储中[0,8191]，[0,4095]代表本身，[4096,8191]实际代表[-4096,-1]。

<img src="./pic/listpack_encode.png" />

listpack.h/listpack.c中相关的代码基本都是对一个`unsigned char *`类型的变量进行操作，利用位运算进行bit尺度的赋值，根据listpack设计规则提供相关接口，如插入，删除，判断，统计等。