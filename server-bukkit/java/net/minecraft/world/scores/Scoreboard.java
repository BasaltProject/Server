package net.minecraft.world.scores;
import java.util.Collection;
import java.util.LinkedHashMap;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.numbers.NumberFormat;
import net.minecraft.world.scores.criteria.ObjectiveCriteria;
public class Scoreboard {
    private final java.util.Map<String, Objective> objectives = new LinkedHashMap<>();
    public Objective getObjective(String name) {
        return objectives.get(name);
    }
    public Collection<Objective> getObjectives() {
        return java.util.Collections.unmodifiableCollection(objectives.values());
    }
    public Objective addObjective(String name, ObjectiveCriteria criteria, Component display,
        ObjectiveCriteria.RenderType render, boolean autoUpdate, NumberFormat format) {
        if (objectives.containsKey(name))
            throw new IllegalArgumentException("Duplicate objective: " + name);
        Objective objective = new Objective(this, name, criteria, display, render, autoUpdate, format);
        objectives.put(name, objective);
        return objective;
    }
    public void removeObjective(Objective objective) {
        objectives.remove(objective.getName(), objective);
    }
}
