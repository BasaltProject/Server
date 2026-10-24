package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface SchoolableFishOps extends io.papermc.paper.entity.SchoolableFish, Cored {
    default void startFollowing(io.papermc.paper.entity.SchoolableFish leader) {
        core().call("school_follow", (long) leader.getEntityId());
    }

    default void stopFollowing() {
        core().call("school_stop");
    }

    private String[] school() {
        return core().call("school_info") instanceof String[] facts && facts.length == 3 ? facts
            : new String[] {"-1", "1", "8"};
    }

    default int getSchoolSize() {
        return Integer.parseInt(school()[1]);
    }

    default int getMaxSchoolSize() {
        return Integer.parseInt(school()[2]);
    }

    default io.papermc.paper.entity.SchoolableFish getSchoolLeader() {
        int id = Integer.parseInt(school()[0]);
        return id >= 0 && core().entity(id) instanceof io.papermc.paper.entity.SchoolableFish leader ? leader : null;
    }
}
