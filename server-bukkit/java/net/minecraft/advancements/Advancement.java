// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;
import net.minecraft.ChatFormatting;
import net.minecraft.advancements.triggers.Criterion;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.chat.Style;
import net.minecraft.network.chat.HoverEvent;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
public record Advancement(Optional<Identifier> parent, Optional<DisplayInfo> display, AdvancementRewards rewards, Map<String, Criterion<?>> criteria, AdvancementRequirements requirements, boolean sendsTelemetryEvent, Optional<Component> name) {
    private static final Codec<Map<String, Criterion<?>>> kCriteria = Codec.unboundedMap(Codec.STRING, Criterion.CODEC)
        .validate(criteria -> criteria.isEmpty() ? DataResult.error(() -> "Advancement criteria cannot be empty") : DataResult.success(criteria));
    public static final Codec<Advancement> CODEC = RecordCodecBuilder.<Advancement>create(i -> i.group(
        Identifier.CODEC.optionalFieldOf("parent").forGetter(Advancement::parent),
        DisplayInfo.CODEC.optionalFieldOf("display").forGetter(Advancement::display),
        AdvancementRewards.CODEC.optionalFieldOf("rewards", AdvancementRewards.EMPTY).forGetter(Advancement::rewards),
        kCriteria.fieldOf("criteria").forGetter(Advancement::criteria),
        AdvancementRequirements.CODEC.optionalFieldOf("requirements").forGetter(advancement -> Optional.of(advancement.requirements())),
        Codec.BOOL.optionalFieldOf("sends_telemetry_event", false).forGetter(Advancement::sendsTelemetryEvent))
        .apply(i, (parent, display, rewards, criteria, requirements, telemetry) -> new Advancement(parent, display, rewards, criteria,
            requirements.orElseGet(() -> AdvancementRequirements.allOf(criteria.keySet())), telemetry)))
        .validate(Advancement::validate);
    public static final StreamCodec<RegistryFriendlyByteBuf, Advancement> STREAM_CODEC = StreamCodec.of(
        (buffer, advancement) -> {
            throw new UnsupportedOperationException("Advancements are sent by the native server");
        }, buffer -> {
            throw new UnsupportedOperationException("Advancements are sent by the native server");
        });
    public Advancement(Optional<Identifier> parent, Optional<DisplayInfo> display, AdvancementRewards rewards, Map<String, Criterion<?>> criteria, AdvancementRequirements requirements, boolean sendsTelemetryEvent) {
        this(parent, display, rewards, new LinkedHashMap<>(criteria), requirements, sendsTelemetryEvent, display.map(Advancement::decorateName));
    }
    private static DataResult<Advancement> validate(Advancement advancement) {
        boolean isRoot = advancement.parent.isEmpty();
        boolean visible = advancement.display.isPresent();
        boolean background = visible && advancement.display.get().background().isPresent();
        if (isRoot && visible && !background) {
            return DataResult.error(() -> "Visible advancement roots must have background");
        }
        if (!isRoot && background) {
            return DataResult.error(() -> "Only advancement roots can have background");
        }
        DataResult<AdvancementRequirements> checked = advancement.requirements().validate(advancement.criteria().keySet());
        return checked.isError() ? checked.map(r -> advancement) : DataResult.success(advancement);
    }
    private static Component decorateName(DisplayInfo display) {
        ChatFormatting color = display.type().getChatColor();
        MutableComponent tooltip = display.title().copy().withStyle(Style.EMPTY.withColor(color)).append("\n").append(display.description());
        MutableComponent title = display.title().copy().withStyle(style -> style.withHoverEvent(new HoverEvent.ShowText(tooltip)));
        return Component.literal("[").append(title).append("]").withStyle(color);
    }
    public static Component name(AdvancementHolder holder) {
        return holder.value().name().orElseGet(() -> Component.literal(holder.id().toString()));
    }
    public boolean isRoot() {
        return parent.isEmpty();
    }
    public static class Builder {
        private Optional<Identifier> parent = Optional.empty();
        private Optional<DisplayInfo> display = Optional.empty();
        private AdvancementRewards rewards = AdvancementRewards.EMPTY;
        private final Map<String, Criterion<?>> criteria = new LinkedHashMap<>();
        private Optional<AdvancementRequirements> requirements = Optional.empty();
        private AdvancementRequirements.Strategy strategy = AdvancementRequirements.Strategy.AND;
        private boolean sendsTelemetryEvent;
        public static Builder advancement() {
            return new Builder().sendsTelemetryEvent();
        }
        public static Builder recipeAdvancement() {
            return new Builder();
        }
        public Builder parent(AdvancementHolder parent) {
            return parent(parent.id());
        }
        public Builder parent(Identifier parent) {
            this.parent = Optional.of(parent);
            return this;
        }
        public Builder display(DisplayInfo display) {
            this.display = Optional.of(display);
            return this;
        }
        public Builder rewards(AdvancementRewards.Builder rewards) {
            return rewards(rewards.build());
        }
        public Builder rewards(AdvancementRewards rewards) {
            this.rewards = rewards;
            return this;
        }
        public Builder addCriterion(String name, Criterion<?> criterion) {
            criteria.put(name, criterion);
            return this;
        }
        public Builder requirements(AdvancementRequirements.Strategy strategy) {
            this.strategy = strategy;
            return this;
        }
        public Builder requirements(AdvancementRequirements requirements) {
            this.requirements = Optional.of(requirements);
            return this;
        }
        public Builder sendsTelemetryEvent() {
            sendsTelemetryEvent = true;
            return this;
        }
        public Advancement build() {
            AdvancementRequirements built = requirements.orElseGet(() -> strategy.create(criteria.keySet()));
            return new Advancement(parent, display, rewards, criteria, built, sendsTelemetryEvent);
        }
        public AdvancementHolder build(Identifier id) {
            return new AdvancementHolder(id, build());
        }
    }
}
