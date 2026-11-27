package nre.kt.dsl

import nre.Vec2
import nre.Vec3
import nre.Vertex
import nre.kt.vec3
import org.junit.jupiter.api.Assertions.assertArrayEquals
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Test

/** mesh { }'s builder, without a library: the vertices and indices it would pass to createMeshModel. */
class MeshBuilderTest {
    @Test
    fun verticesGetIndicesAndQuadsAreTwoTriangles() {
        val b = MeshBuilder().apply {
            val a = vertex(vec3(0, 0, 0))
            val c = vertex(vec3(1, 0, 0), normal = Vec3.UP, uv = Vec2(1f, 0f))
            val d = vertex(vec3(1, 1, 0))
            val e = vertex(vec3(0, 1, 0))
            assertEquals(listOf(0, 1, 2, 3), listOf(a, c, d, e))
            quad(a, c, d, e)
        }
        assertEquals(4, b.vertexCount)
        assertEquals(Vertex(vec3(1, 0, 0), Vec3.UP, Vec2(1f, 0f)), b.vertices[1])
        assertEquals(Vertex(vec3(0, 0, 0)), b.vertices[0])
        assertArrayEquals(intArrayOf(0, 1, 2, 0, 2, 3), b.indexArray())
    }

    @Test
    fun manyTriangles() {
        val b = MeshBuilder().apply {
            repeat(3) { vertex(Vec3.ZERO) }
            repeat(1000) { triangle(0, 1, 2) }
        }
        val indices = b.indexArray()
        assertEquals(3000, indices.size)
        assertArrayEquals(intArrayOf(0, 1, 2), indices.copyOfRange(2997, 3000))
    }
}
