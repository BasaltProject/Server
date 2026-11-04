// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent.vibrations;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;

public class VibrationSelector {
    public static final Codec<VibrationSelector> CODEC = RecordCodecBuilder.create(instance -> instance.group(
        VibrationInfo.CODEC.lenientOptionalFieldOf("event").forGetter(selector -> selector.current.map(VibrationEvent::event)),
        Codec.LONG.fieldOf("tick").forGetter(selector -> selector.current.map(VibrationEvent::tick).orElse(-1L)))
        .apply(instance, VibrationSelector::new));

    private Optional<VibrationEvent> current;

    public VibrationSelector(Optional<VibrationInfo> currentVibration, long tick) {
        this.current = currentVibration.map(info -> new VibrationEvent(info, tick));
    }

    public VibrationSelector() {
        this.current = Optional.empty();
    }

    public void addCandidate(VibrationInfo newVibration, long tickTime) {
        if (beats(newVibration, tickTime)) {
            current = Optional.of(new VibrationEvent(newVibration, tickTime));
        }
    }

    private boolean beats(VibrationInfo candidate, long tickTime) {
        if (current.isEmpty()) {
            return true;
        }
        VibrationEvent held = current.get();
        if (held.tick() != tickTime) {
            return false;
        }
        if (candidate.distance() != held.event().distance()) {
            return candidate.distance() < held.event().distance();
        }
        return VibrationSystem.getGameEventFrequency(candidate.gameEvent()) > VibrationSystem.getGameEventFrequency(held.event().gameEvent());
    }

    public Optional<VibrationInfo> chosenCandidate(long time) {
        return current.filter(held -> held.tick() < time).map(VibrationEvent::event);
    }

    public void startOver() {
        current = Optional.empty();
    }

    static record VibrationEvent(VibrationInfo event, long tick) {
    }
}
