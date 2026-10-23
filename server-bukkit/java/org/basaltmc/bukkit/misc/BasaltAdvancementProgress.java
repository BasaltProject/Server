package org.basaltmc.bukkit.misc;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.advancement.Advancement;
import org.bukkit.advancement.AdvancementProgress;

public final class BasaltAdvancementProgress implements AdvancementProgress {
    private final String uuid;
    private final BasaltAdvancement advancement;

    public BasaltAdvancementProgress(String uuid, BasaltAdvancement advancement) {
        this.uuid = uuid;
        this.advancement = advancement;
    }

    private String id() {
        return advancement.getKey().toString();
    }

    private Map<String, Long> obtained() {
        String[] flat = MiscNatives.advObtained(uuid, id());
        Map<String, Long> out = new HashMap<>();
        for (int i = 0; i + 1 < flat.length; i += 2) {
            out.put(flat[i], Long.parseLong(flat[i + 1]));
        }
        return out;
    }

    @Override
    public Advancement getAdvancement() {
        return advancement;
    }

    @Override
    public boolean isDone() {
        Map<String, Long> got = obtained();
        for (String group : MiscNatives.advLists(id(), 2)) {
            boolean met = false;
            for (String name : group.split("\n")) {
                met |= got.containsKey(name);
            }
            if (!met) {
                return false;
            }
        }
        return true;
    }

    @Override
    public boolean awardCriteria(String criteria) {
        return MiscNatives.advGrant(uuid, id(), criteria);
    }

    @Override
    public boolean revokeCriteria(String criteria) {
        return MiscNatives.advRevoke(uuid, id(), criteria);
    }

    @Override
    public Date getDateAwarded(String criteria) {
        Long when = obtained().get(criteria);
        return when == null ? null : new Date(when);
    }

    @Override
    public Collection<String> getRemainingCriteria() {
        Map<String, Long> got = obtained();
        List<String> out = new ArrayList<>();
        for (String name : MiscNatives.advLists(id(), 0)) {
            if (!got.containsKey(name)) {
                out.add(name);
            }
        }
        return out;
    }

    @Override
    public Collection<String> getAwardedCriteria() {
        return new ArrayList<>(obtained().keySet());
    }
}
