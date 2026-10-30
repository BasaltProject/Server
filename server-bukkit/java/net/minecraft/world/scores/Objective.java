package net.minecraft.world.scores;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.numbers.NumberFormat;
import net.minecraft.world.scores.criteria.ObjectiveCriteria;
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
}
