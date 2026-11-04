// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import io.netty.buffer.ByteBuf;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
public record AdvancementRequirements(List<List<String>> requirements) {
    public static final Codec<AdvancementRequirements> CODEC = Codec.STRING.listOf().listOf().xmap(AdvancementRequirements::new, AdvancementRequirements::requirements);
    public static final StreamCodec<ByteBuf, AdvancementRequirements> STREAM_CODEC =
        ByteBufCodecs.STRING_UTF8.apply(ByteBufCodecs.list()).apply(ByteBufCodecs.list()).map(AdvancementRequirements::new, AdvancementRequirements::requirements);
    public static final AdvancementRequirements EMPTY = new AdvancementRequirements(List.of());
    public static AdvancementRequirements allOf(Collection<String> criteria) {
        return new AdvancementRequirements(criteria.stream().map(List::of).toList());
    }
    public static AdvancementRequirements anyOf(Collection<String> criteria) {
        return new AdvancementRequirements(List.of(List.copyOf(criteria)));
    }
    public int size() {
        return requirements.size();
    }
    public boolean test(Predicate<String> predicate) {
        if (requirements.isEmpty()) {
            return false;
        }
        for (List<String> group : requirements) {
            if (group.stream().noneMatch(predicate)) {
                return false;
            }
        }
        return true;
    }
    public int count(Predicate<String> predicate) {
        int count = 0;
        for (List<String> group : requirements) {
            if (group.stream().anyMatch(predicate)) {
                count++;
            }
        }
        return count;
    }
    public DataResult<AdvancementRequirements> validate(Set<String> expected) {
        Set<String> referenced = new LinkedHashSet<>();
        for (List<String> group : requirements) {
            if (group.isEmpty() && expected.isEmpty()) {
                return DataResult.error(() -> "Requirement entry cannot be empty");
            }
            referenced.addAll(group);
        }
        if (!expected.equals(referenced)) {
            Set<String> missing = new LinkedHashSet<>(expected);
            missing.removeAll(referenced);
            Set<String> unknown = new LinkedHashSet<>(referenced);
            unknown.removeAll(expected);
            return DataResult.error(() -> "Advancement completion requirements did not exactly match specified criteria. Missing: " + missing + ". Unknown: " + unknown);
        }
        return DataResult.success(this);
    }
    public boolean isEmpty() {
        return requirements.isEmpty();
    }
    @Override
    public String toString() {
        return requirements.toString();
    }
    public Set<String> names() {
        Set<String> names = new LinkedHashSet<>();
        for (List<String> group : requirements) {
            names.addAll(group);
        }
        return names;
    }
    public interface Strategy {
        Strategy AND = AdvancementRequirements::allOf;
        Strategy OR = AdvancementRequirements::anyOf;
        AdvancementRequirements create(Collection<String> criteria);
    }
}
