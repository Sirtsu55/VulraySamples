#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera 
{

public:
    // Constructor
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),
           glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
           float fov = 45.0f, float aspectRatio = 16.0f / 9.0f,
           float nearPlane = 0.1f, float farPlane = 100.0f);

    // Getters
    glm::mat4 GetViewMatrix();
    glm::mat4 GetProjectionMatrix();

    // Movement
    void MoveRight(float distance);
    void MoveForward(float distance);

    void Rotate(float pitch, float yaw, float roll);
    void Rotate(const glm::quat& rotation);


    glm::vec3 Position;      // Camera position
    glm::quat Rotation;      // Camera rotation
    float Fov;               // Field of view (in degrees)
    float AspectRatio;      // Aspect ratio of the viewport
    float NearPlane;        // Near clipping plane
    float FarPlane;         // Far clipping plane

    float Sensitivity = 75000.0f; // Pitch and Yaw sensitivity

    float RollSensitivity = 100.0f; // Roll sensitivity
    float Speed = 2.5f;       // Camera movement speed

    glm::vec3 Front = glm::vec3(0.0, 0.0, -1.0);
    glm::vec3 Right = glm::vec3(1.0, 0.0, 0.0);
    glm::vec3 Up = glm::vec3(0.0, 1.0, 0.0);

    // When the camera is rotated, the front, right and up vectors need to be recalculated
    // Done automatically by the Rotate function, but can be done manually if needed
    void UpdateDirections();
private:
};
