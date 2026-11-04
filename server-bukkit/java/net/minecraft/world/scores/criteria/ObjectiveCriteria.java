// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores.criteria;

import com.mojang.serialization.Codec;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.scores.TeamColor;
public class ObjectiveCriteria {
    private static final Map<String, ObjectiveCriteria> kCustom = new java.util.concurrent.ConcurrentHashMap<>();
    public static final ObjectiveCriteria DUMMY = new ObjectiveCriteria("dummy");
    public enum RenderType implements StringRepresentable { INTEGER, HEARTS ;
        public static final StringRepresentable.EnumCodec<RenderType> CODEC = StringRepresentable.fromEnum(RenderType::values);
        public String getId() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public String getSerializedName() {
            return getId();
        }
        public static RenderType byId(String key) {
            return "hearts".equals(key) ? HEARTS : INTEGER;
        }
}
    private final String name;
    private final boolean readOnly;
    private final RenderType renderType;

    protected ObjectiveCriteria() {
        this("dummy");
    }
    public static final Codec<ObjectiveCriteria> CODEC = Codec.STRING.comapFlatMap(
        name -> byName(name).map(com.mojang.serialization.DataResult::success)
            .orElseGet(() -> com.mojang.serialization.DataResult.error(() -> "Unknown criteria: " + name)),
        ObjectiveCriteria::getName);
    public static final ObjectiveCriteria TRIGGER = new ObjectiveCriteria("trigger");
    public static final ObjectiveCriteria DEATH_COUNT = new ObjectiveCriteria("deathCount");
    public static final ObjectiveCriteria KILL_COUNT_PLAYERS = new ObjectiveCriteria("playerKillCount");
    public static final ObjectiveCriteria KILL_COUNT_ALL = new ObjectiveCriteria("totalKillCount");
    public static final ObjectiveCriteria HEALTH = new ObjectiveCriteria("health", true, RenderType.HEARTS);
    public static final ObjectiveCriteria FOOD = new ObjectiveCriteria("food", true, RenderType.INTEGER);
    public static final ObjectiveCriteria AIR = new ObjectiveCriteria("air", true, RenderType.INTEGER);
    public static final ObjectiveCriteria ARMOR = new ObjectiveCriteria("armor", true, RenderType.INTEGER);
    public static final ObjectiveCriteria EXPERIENCE = new ObjectiveCriteria("xp", true, RenderType.INTEGER);
    public static final ObjectiveCriteria LEVEL = new ObjectiveCriteria("level", true, RenderType.INTEGER);
    public static final Map<TeamColor, ObjectiveCriteria> TEAM_KILL = teamCriteria("teamkill.");
    public static final Map<TeamColor, ObjectiveCriteria> KILLED_BY_TEAM = teamCriteria("killedByTeam.");

    private static Map<TeamColor, ObjectiveCriteria> teamCriteria(String prefix) {
        Map<TeamColor, ObjectiveCriteria> map = new java.util.EnumMap<>(TeamColor.class);
        for (TeamColor color : TeamColor.values()) {
            map.put(color, new ObjectiveCriteria(prefix + color.getSerializedName()));
        }
        return java.util.Collections.unmodifiableMap(map);
    }
    protected ObjectiveCriteria(String name) {
        this(name, false, RenderType.INTEGER);
    }
    protected ObjectiveCriteria(String name, boolean readOnly, RenderType renderType) {
        this.name = name;
        this.readOnly = readOnly;
        this.renderType = renderType;
        kCustom.put(name, this);
    }
    public static Set<String> getCustomCriteriaNames() {
        return java.util.Set.copyOf(kCustom.keySet());
    }
    public static Optional<ObjectiveCriteria> byName(String name) {
        return Optional.ofNullable(kCustom.get(name));
    }
    public String getName() {
        return name;
    }
    public boolean isReadOnly() {
        return readOnly;
    }
    public RenderType getDefaultRenderType() {
        return renderType;
    }
}
