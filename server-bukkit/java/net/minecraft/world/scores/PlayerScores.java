// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;
import it.unimi.dsi.fastutil.objects.Object2IntMap;
import java.util.Map;
import java.util.function.Consumer;
class PlayerScores {
    private final Map<Objective, Score> scores = new java.util.HashMap<>(16, 0.5F);
    public Score get(Objective objective) {
        return scores.get(objective);
    }
    public Score getOrCreate(Objective objective, Consumer<Score> newResultCallback) {
        return scores.computeIfAbsent(objective, key -> {
            Score fresh = new Score();
            newResultCallback.accept(fresh);
            return fresh;
        });
    }
    public boolean remove(Objective objective) {
        return scores.remove(objective) != null;
    }
    public boolean hasScores() {
        return !scores.isEmpty();
    }
    public Object2IntMap<Objective> listScores() {
        Object2IntMap<Objective> out = new it.unimi.dsi.fastutil.objects.Object2IntOpenHashMap<>();
        scores.forEach((objective, score) -> out.put(objective, score.value()));
        return out;
    }
    void setScore(Objective objective, Score score) {
        scores.put(objective, score);
    }
    Map<Objective, Score> listRawScores() {
        return java.util.Collections.unmodifiableMap(scores);

    }
}
