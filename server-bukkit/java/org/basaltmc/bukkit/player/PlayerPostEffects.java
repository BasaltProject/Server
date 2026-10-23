package org.basaltmc.bukkit.player;

import java.util.ArrayList;
import java.util.List;
import java.util.SequencedCollection;
import net.kyori.adventure.key.Key;
import org.bukkit.entity.Player;

final class PlayerPostEffects implements io.papermc.paper.entity.PlayerPostEffects {
    private final Player player;

    PlayerPostEffects(Player player) {
        this.player = player;
    }

    @Override
    public List<Key> values() {
        List<Key> keys = new ArrayList<>();
        if (PlayerCalls.call(player.getUniqueId(), "post_effects") instanceof String[] held) {
            for (String text : held) {
                keys.add(Key.key(text));
            }
        }
        return keys;
    }

    @Override
    public boolean set(SequencedCollection<Key> effects) {
        List<String> names = new ArrayList<>();
        for (Key effect : effects) {
            names.add(effect.asString());
        }
        return PlayerCalls.done(player.getUniqueId(), "set_post_effects", (Object) names.toArray(new String[0]));
    }

    @Override
    public boolean add(Key effect) {
        List<Key> held = new ArrayList<>(values());
        if (held.contains(effect)) {
            return false;
        }
        held.add(effect);
        return set(held);
    }

    @Override
    public boolean remove(Key effect) {
        List<Key> held = new ArrayList<>(values());
        return held.remove(effect) && set(held);
    }

    @Override
    public boolean clear() {
        return !values().isEmpty() && set(List.of());
    }

    @Override
    public void update() {
        PlayerCalls.call(player.getUniqueId(), "set_post_effects", (Object) values().stream().map(Key::asString)
            .toArray(String[]::new));
    }
}
