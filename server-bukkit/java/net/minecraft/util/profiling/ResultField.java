// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling;
public final class ResultField implements Comparable<ResultField> {
    public final double percentage;
    public final double globalPercentage;
    public final long count;
    public final String name;
    public ResultField(String name, double percentage, double globalPercentage, long count) {
        this.name = name;
        this.percentage = percentage;
        this.globalPercentage = globalPercentage;
        this.count = count;
    }
    @Override
    public int compareTo(ResultField other) {
        if (other.percentage < percentage) {
            return -1;
        }
        return other.percentage > percentage ? 1 : other.name.compareTo(name);
    }
    public int getColor() {
        return (name.hashCode() & 11184810) + 4473924;
    }
}
