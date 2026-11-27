package nre;

import java.util.Arrays;

/**
 * A 4x4 float matrix, like {@code NreMat4} / {@code glm::mat4}: column-major, element (column, row) is
 * {@code m[column * 4 + row]}. Immutable: the array is copied in and out.
 */
public final class Mat4 {
    public static final Mat4 IDENTITY = new Mat4(new float[] {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1,
    });

    private final float[] m;

    /** @param m 16 floats, column-major; copied. */
    public Mat4(float[] m) {
        if (m.length != 16) throw new IllegalArgumentException("Mat4 needs 16 floats, got " + m.length);
        this.m = m.clone();
    }

    /** The element in column {@code column}, row {@code row} (both 0..3). */
    public float get(int column, int row) {
        if (column < 0 || column > 3 || row < 0 || row > 3) {
            throw new IndexOutOfBoundsException("column " + column + ", row " + row);
        }
        return m[column * 4 + row];
    }

    /** A copy of the 16 floats, column-major. */
    public float[] toArray() { return m.clone(); }

    @Override public boolean equals(Object o) { return o instanceof Mat4 other && Arrays.equals(m, other.m); }
    @Override public int hashCode() { return Arrays.hashCode(m); }
    @Override public String toString() { return "Mat4" + Arrays.toString(m); }
}
