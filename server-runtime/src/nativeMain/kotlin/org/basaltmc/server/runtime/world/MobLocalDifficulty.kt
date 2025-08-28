package org.basaltmc.server.runtime.world

/** Minecraft's local difficulty multiplier consumed by concrete mob finalizers. */
internal object MobLocalDifficulty {
    fun specialMultiplier(
        difficulty: String,
        gameTime: Long,
        inhabitedTime: Long,
        moonBrightness: Float,
    ): Float {
        val effectiveDifficulty = effectiveDifficulty(difficulty, gameTime, inhabitedTime, moonBrightness)
        return when {
            effectiveDifficulty < 2f -> 0f
            effectiveDifficulty > 4f -> 1f
            else -> (effectiveDifficulty - 2f) / 2f
        }
    }

    /** parity: `DifficultyInstance.getEffectiveDifficulty`, also used by custom spawners. */
    fun effectiveDifficulty(
        difficulty: String,
        gameTime: Long,
        inhabitedTime: Long,
        moonBrightness: Float,
    ): Float {
        val difficultyId =
            when (difficulty) {
                "peaceful" -> 0
                "easy" -> 1
                "normal" -> 2
                "hard" -> 3
                else -> 1
            }
        if (difficultyId == 0) return 0f

        val globalBonus =
            ((gameTime.toFloat() - GLOBAL_TIME_OFFSET) / MAX_GLOBAL_TIME)
                .coerceIn(0f, 1f) * GLOBAL_TIME_WEIGHT
        var localScale =
            (inhabitedTime.toFloat() / MAX_LOCAL_TIME).coerceIn(0f, 1f) *
                if (difficulty == "hard") 1f else 0.75f
        localScale += (moonBrightness * MOON_WEIGHT).coerceIn(0f, globalBonus)
        if (difficulty == "easy") localScale *= 0.5f
        return difficultyId * (BASE_SCALE + globalBonus + localScale)
    }

    private const val GLOBAL_TIME_OFFSET = 72_000f
    private const val MAX_GLOBAL_TIME = 1_440_000f
    private const val MAX_LOCAL_TIME = 3_600_000f
    private const val BASE_SCALE = 0.75f
    private const val GLOBAL_TIME_WEIGHT = 0.25f
    private const val MOON_WEIGHT = 0.25f
}
