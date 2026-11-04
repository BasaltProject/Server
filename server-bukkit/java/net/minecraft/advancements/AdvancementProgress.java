// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import io.netty.buffer.ByteBuf;
import java.time.Instant;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.stream.Collectors;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.ExtraCodecs;
public class AdvancementProgress implements Comparable<AdvancementProgress> {
    private static final DateTimeFormatter kObtainedFormat = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss Z", Locale.ROOT);
    private static final Codec<Instant> kObtainedCodec = ExtraCodecs.temporalCodec(kObtainedFormat)
        .xmap(Instant::from, instant -> instant.atZone(ZoneId.systemDefault()));
    private static final Codec<Map<String, CriterionProgress>> kCriteriaCodec = Codec.unboundedMap(Codec.STRING, kObtainedCodec)
        .xmap(map -> {
            Map<String, CriterionProgress> out = new HashMap<>();
            map.forEach((name, when) -> out.put(name, new CriterionProgress(when)));
            return out;
        }, map -> map.entrySet().stream().filter(entry -> entry.getValue().isDone())
            .collect(Collectors.toMap(Map.Entry::getKey, entry -> Objects.requireNonNull(entry.getValue().getObtained()))));
    public static final Codec<AdvancementProgress> CODEC = RecordCodecBuilder.create(i -> i.group(
        kCriteriaCodec.optionalFieldOf("criteria", Map.of()).forGetter(progress -> progress.criteria),
        ExtraCodecs.optionalAlwaysPresentFieldOf(Codec.BOOL, "done", true).forGetter(AdvancementProgress::isDone))
        .apply(i, (criteria, done) -> new AdvancementProgress(new HashMap<>(criteria))));
    public static final StreamCodec<ByteBuf, AdvancementProgress> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.map(HashMap::new, ByteBufCodecs.STRING_UTF8, CriterionProgress.STREAM_CODEC), progress -> progress.criteria,
        AdvancementProgress::new);
    private final Map<String, CriterionProgress> criteria;
    private AdvancementRequirements requirements = AdvancementRequirements.EMPTY;
    private AdvancementProgress(Map<String, CriterionProgress> criteria) {
        this.criteria = criteria;
    }
    public AdvancementProgress() {
        this.criteria = new HashMap<>();
    }
    public static AdvancementProgress of(AdvancementRequirements requirements, Map<String, Instant> obtained) {
        AdvancementProgress progress = new AdvancementProgress();
        progress.update(requirements);
        obtained.forEach((name, when) -> {
            if (progress.criteria.containsKey(name)) {
                progress.criteria.put(name, new CriterionProgress(when));
            }
        });
        return progress;
    }
    public void update(AdvancementRequirements requirements) {
        Set<String> names = requirements.names();
        criteria.entrySet().removeIf(entry -> !names.contains(entry.getKey()));
        for (String name : names) {
            criteria.putIfAbsent(name, new CriterionProgress());
        }
        this.requirements = requirements;
    }
    public boolean isDone() {
        return requirements.test(this::isCriterionDone);
    }
    public boolean hasProgress() {
        return criteria.values().stream().anyMatch(CriterionProgress::isDone);
    }
    public boolean grantProgress(String name) {
        CriterionProgress progress = criteria.get(name);
        if (progress != null && !progress.isDone()) {
            progress.grant();
            return true;
        }
        return false;
    }
    public boolean revokeProgress(String name) {
        CriterionProgress progress = criteria.get(name);
        if (progress != null && progress.isDone()) {
            progress.revoke();
            return true;
        }
        return false;
    }
    @Override
    public String toString() {
        return "AdvancementProgress{criteria=" + criteria + ", requirements=" + requirements + "}";
    }
    public CriterionProgress getCriterion(String id) {
        return criteria.get(id);
    }
    private boolean isCriterionDone(String criterion) {
        CriterionProgress progress = getCriterion(criterion);
        return progress != null && progress.isDone();
    }
    public float getPercent() {
        if (criteria.isEmpty()) {
            return 0.0F;
        }
        return (float) requirements.count(this::isCriterionDone) / (float) requirements.size();
    }
    public Component getProgressText() {
        if (criteria.isEmpty() || requirements.size() <= 1) {
            return null;
        }
        return Component.translatable("advancements.progress", requirements.count(this::isCriterionDone), requirements.size());
    }
    public Iterable<String> getRemainingCriteria() {
        List<String> out = new ArrayList<>();
        criteria.forEach((name, progress) -> {
            if (!progress.isDone()) {
                out.add(name);
            }
        });
        return out;
    }
    public Iterable<String> getCompletedCriteria() {
        List<String> out = new ArrayList<>();
        criteria.forEach((name, progress) -> {
            if (progress.isDone()) {
                out.add(name);
            }
        });
        return out;
    }
    public Instant getFirstProgressDate() {
        return criteria.values().stream().map(CriterionProgress::getObtained).filter(Objects::nonNull).min(Comparator.naturalOrder()).orElse(null);
    }
    @Override
    public int compareTo(AdvancementProgress other) {
        Instant ours = getFirstProgressDate();
        Instant theirs = other.getFirstProgressDate();
        if (ours == null) {
            return theirs == null ? 0 : 1;
        }
        return theirs == null ? -1 : ours.compareTo(theirs);
    }
}
