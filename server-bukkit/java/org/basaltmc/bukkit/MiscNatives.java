package org.basaltmc.bukkit;

public final class MiscNatives {
    private MiscNatives() {}

    public static native byte[] worldData(String world);
    public static native void setWorldData(String world, byte[] nbt);
    public static native byte[] chunkData(String world, int chunkX, int chunkZ);
    public static native void setChunkData(String world, int chunkX, int chunkZ, byte[] nbt);
    public static native byte[] blockEntityData(String world, int x, int y, int z);
    public static native void setBlockEntityData(String world, int x, int y, int z, byte[] nbt);
    public static native byte[] entityData(String world, int entityId);
    public static native void setEntityData(String world, int entityId, byte[] nbt);
    public static native byte[] playerData(String uuid);
    public static native void setPlayerData(String uuid, byte[] nbt);

    public static native void playerPermissions(String uuid, String[] nodes, boolean[] values);

    public static native String[] knownPlayer(String key, boolean byName);
    public static native double[] knownPlayerNumbers(String uuid);
    public static native String[] knownPlayers();
    public static native boolean isOperator(String uuid);
    public static native boolean setOperator(String uuid, boolean value);
    public static native String[] operators();
    public static native boolean isWhitelisted(String uuid);
    public static native void setWhitelisted(String uuid, String name, boolean value);
    public static native String[] whitelisted();
    public static native boolean whitelistEnforced();
    public static native void setWhitelistEnforced(boolean value);
    public static native String[] banOf(String subject, boolean ip);
    public static native long[] banTimes(String subject, boolean ip);
    public static native String[] bans(boolean ip);
    public static native void ban(String subject, String name, String reason, String source, long created, long expires,
        boolean ip);
    public static native boolean pardon(String subject, boolean ip);

    public static native int sbCreate();
    public static native void sbRelease(int board);
    public static native String[] sbObjectives(int board);
    public static native String[] sbObjective(int board, String name);
    public static native boolean sbAddObjective(int board, String[] fields);
    public static native boolean sbUpdateObjective(int board, String[] fields);
    public static native boolean sbRemoveObjective(int board, String name);
    public static native String sbDisplayed(int board, int slot);
    public static native void sbDisplay(int board, int slot, String objective);
    public static native long sbScore(int board, String entry, String objective);
    public static native boolean sbSetScore(int board, String entry, String objective, int value);
    public static native void sbResetScore(int board, String entry, String objective);
    public static native String[] sbEntries(int board);
    public static native String[] sbEntriesIn(int board, String objective);
    public static native String[] sbScoresOf(int board, String entry);
    public static native String[] sbScoreDisplay(int board, String entry, String objective);
    public static native void sbSetScoreDisplay(int board, String entry, String objective, String[] display);
    public static native String[] sbTeams(int board);
    public static native String[] sbTeam(int board, String name);
    public static native String[] sbTeamOf(int board, String entry);
    public static native boolean sbAddTeam(int board, String name);
    public static native boolean sbUpdateTeam(int board, String[] fields);
    public static native boolean sbRemoveTeam(int board, String name);
    public static native boolean sbAddMember(int board, String team, String entry);
    public static native boolean sbRemoveMember(int board, String team, String entry);
    public static native void sbShow(String uuid, int board);
    public static native int sbShown(String uuid);
    public static native boolean sbTriggerable(int board, String entry, String objective);
    public static native void sbSetTriggerable(int board, String entry, String objective, boolean value);
    public static native String bbCreate(String[] fields);
    public static native String[] bbGet(String id);
    public static native String[] bbKept();
    public static native boolean bbUpdate(String[] fields);
    public static native boolean bbRemove(String id);
    public static native boolean bbAddPlayer(String id, String uuid);
    public static native boolean bbRemovePlayer(String id, String uuid);
    public static native void bbClear(String id);
    public static native String[] bbPlayers(String id);
    public static native String[] advIds();
    public static native String[] advInfo(String id);
    public static native String[] advLists(String id, int which);
    public static native String[] advObtained(String uuid, String id);
    public static native boolean advGrant(String uuid, String id, String criterion);
    public static native boolean advRevoke(String uuid, String id, String criterion);
    public static native float tmRate();
    public static native void tmSetRate(float rate);
    public static native boolean tmFrozen();
    public static native void tmSetFrozen(boolean frozen);
    public static native boolean tmStep(int ticks);
    public static native int tmStepsOwed();
    public static native boolean tmStopStepping();
    public static native boolean tmSprint(int ticks);
    public static native boolean tmSprinting();
    public static native boolean tmStopSprinting();
    public static native String[] linkList();
    public static native void linkSet(String[] flat);
    public static native String[] dpList();
    public static native void dpRefresh();
    public static native String[] mapInfo(int id);
    public static native int mapCreate(String world, int centerX, int centerZ, int scale);
    public static native boolean mapUpdate(int id, String[] info);
    public static native byte[] mapColors(int id);
    public static native String[] mapCarriers(int id);
    public static native void mapShow(String uuid, int id, byte[] colors, int[] cursors, String[] captions);
    public static native double[] tmPace();
    public static native String[] resourcePack();
    public static native boolean lootHas(String table);
    public static native String[] lootTables();
    public static native String[] lootRoll(String table, String world, double x, double y, double z, float luck,
        long seed, String looter);
}
