// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.numbers.NumberFormat;
import net.minecraft.world.scores.criteria.ObjectiveCriteria;
import com.mojang.serialization.Codec;
import java.util.Optional;
public class Objective {
    private final Scoreboard scoreboard;
    private final String name;
    private final ObjectiveCriteria criteria;
    private Component displayName;
    private ObjectiveCriteria.RenderType renderType;
    private boolean displayAutoUpdate;
    private NumberFormat numberFormat;
    public Objective(Scoreboard scoreboard, String name, ObjectiveCriteria criteria, Component displayName,
        ObjectiveCriteria.RenderType renderType, boolean displayAutoUpdate, NumberFormat numberFormat) {
        this.scoreboard = scoreboard;
        this.name = java.util.Objects.requireNonNull(name);
        this.criteria = criteria;
        this.displayName = java.util.Objects.requireNonNull(displayName);
        this.renderType = java.util.Objects.requireNonNull(renderType);
        this.displayAutoUpdate = displayAutoUpdate;
        this.numberFormat = numberFormat;
    }
    public Scoreboard getScoreboard() {
        return scoreboard;
    }
    public String getName() {
        return name;
    }
    public ObjectiveCriteria getCriteria() {
        return criteria;
    }
    public Component getDisplayName() {
        return displayName;
    }
    public void setDisplayName(Component value) {
        displayName = java.util.Objects.requireNonNull(value);
    }
    public ObjectiveCriteria.RenderType getRenderType() {
        return renderType;
    }
    public void setRenderType(ObjectiveCriteria.RenderType value) {
        renderType = java.util.Objects.requireNonNull(value);
    }
    public boolean displayAutoUpdate() {
        return displayAutoUpdate;
    }
    public void setDisplayAutoUpdate(boolean value) {
        displayAutoUpdate = value;
    }
    public NumberFormat numberFormat() {
        return numberFormat;
    }
    public void setNumberFormat(NumberFormat value) {
        numberFormat = value;
    }
    public Packed pack() {
        return new Packed(name, criteria, displayName, renderType, displayAutoUpdate, Optional.ofNullable(numberFormat));
    }
    public NumberFormat numberFormatOrDefault(NumberFormat _default) {
        return numberFormat == null ? _default : numberFormat;
    }
    public Component getFormattedDisplayName() {
        return Component.literal("[").append(displayName).append("]");
    }
    public static record Packed(String name, ObjectiveCriteria criteria, Component displayName, ObjectiveCriteria.RenderType renderType, boolean displayAutoUpdate, Optional<NumberFormat> numberFormat) {
        public static final Codec<Packed> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.scores.Objective.Packed.class, "CODEC");
    }
}
