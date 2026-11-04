// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;
public interface EntityTypeTest<B, T extends B> {
    static <B, T extends B> EntityTypeTest<B, T> forClass(Class<T> cls) {
        return new EntityTypeTest<>() {
            @Override
            public T tryCast(B entity) {
                return cls.isInstance(entity) ? cls.cast(entity) : null;
            }
        
            @Override
            public Class<? extends B> getBaseClass() {
                return cls;
            }
        };
    }
    static <B, T extends B> EntityTypeTest<B, T> forExactClass(Class<T> cls) {
        return new EntityTypeTest<>() {
            @Override
            public T tryCast(B entity) {
                return cls.equals(entity.getClass()) ? cls.cast(entity) : null;
            }
        
            @Override
            public Class<? extends B> getBaseClass() {
                return cls;
            }
        };
    }
    default T tryCast(B entity) {
        return null;
    }
    default Class<? extends B> getBaseClass() {
        return null;
    }
}
