#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

class Camera {
private:
	float fov;
	float znear, zfar;

	void updateViewMatrix() {
		glm::mat4 rotM = glm::mat4();
		glm::mat4 transM;

		rotM = glm::rotate(rotM, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		transM = glm::translate(glm::mat4(), position);

		matrices.view = transM*rotM;
	};
public:
	glm::vec3 rotation = glm::vec3();
	glm::vec3 position = glm::vec3();

	float rotationSpeed = 1.0f;
	float movementSpeed = 1.0f;

	struct {
		glm::mat4 perspective;
		glm::mat4 view;
	} matrices;

	struct {
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
	} keys;

	bool moving() {
		return keys.left || keys.right || keys.up || keys.down;
	}

	void setPerspective(float fov, float aspect, float znear, float zfar) {
		this->fov = fov;
		this->znear = znear;
		this->zfar = zfar;
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	};

	void updateAspectRatio(float aspect) {
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	}

	void setRotation(glm::vec3 rotation) {
		this->rotation = rotation;
		updateViewMatrix();
	};

	void rotate(glm::vec3 delta) {
		this->rotation += delta;
		updateViewMatrix();
	}

	void setTranslation(glm::vec3 translation) {
		this->position = translation;
		updateViewMatrix();
	};

	void translate(glm::vec3 delta) {
		this->position += delta;
		updateViewMatrix();
	}

	void update(float deltaTime) {
		if (moving()) {
			glm::vec3 camFront = { matrices.view[0].z, matrices.view[1].z, matrices.view[2].z };

			float moveSpeed = deltaTime * movementSpeed;

			if (keys.up)
				position += camFront * moveSpeed;
			if (keys.down)
				position -= camFront * moveSpeed;
			if (keys.left)
				position -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
			if (keys.right)
				position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

			updateViewMatrix();
		}
	};
};