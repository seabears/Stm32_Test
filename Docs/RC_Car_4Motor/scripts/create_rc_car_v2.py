import os

import FreeCAD as App
import Part
import Mesh


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "outputs")
STL = os.path.join(OUT, "rc_car_v2_stl")
os.makedirs(STL, exist_ok=True)

doc = App.newDocument("RC_Car_4Motor_Steering")


def rounded_plate(length, width, radius, height, z=0):
    x0, y0 = -length / 2, -width / 2
    shape = Part.makeBox(length - 2 * radius, width, height, App.Vector(x0 + radius, y0, z))
    shape = shape.fuse(Part.makeBox(length, width - 2 * radius, height, App.Vector(x0, y0 + radius, z)))
    for x in (x0 + radius, x0 + length - radius):
        for y in (y0 + radius, y0 + width - radius):
            shape = shape.fuse(Part.makeCylinder(radius, height, App.Vector(x, y, z)))
    return shape.removeSplitter()


def feature(name, label, shape, color):
    obj = doc.addObject("PartDesign::Feature", name)
    obj.Label = label
    obj.Shape = shape
    if obj.ViewObject:
        obj.ViewObject.ShapeColor = color
    return obj


def mirror_y(shape):
    matrix = App.Matrix()
    matrix.A22 = -1
    return shape.transformGeometry(matrix)


def motor_pod_local(steering=True):
    # Left-side pod: motor shaft points toward negative Y.
    base = rounded_plate(30, 28, 4, 4, 6)
    # Motor tunnel: 18 x 26 x 16 mm with a 12.4 mm square N20 cavity.
    housing = Part.makeBox(18, 26, 16, App.Vector(-9, -13, 10))
    cavity = Part.makeBox(12.4, 24, 12.4, App.Vector(-6.2, -10, 12))
    housing = housing.cut(cavity)
    # 3.6 mm shaft clearance and diagonal M1.6 face screws (~9 mm spacing).
    housing = housing.cut(Part.makeCylinder(1.8, 5, App.Vector(0, -14, 18.2), App.Vector(0, 1, 0)))
    for x, z in ((-3.18, 15.02), (3.18, 21.38)):
        housing = housing.cut(Part.makeCylinder(0.95, 5, App.Vector(x, -14, z), App.Vector(0, 1, 0)))
    pod = base.fuse(housing)

    if steering:
        # M3 kingpin at wheel-axis centre and rear-facing steering arm.
        pod = pod.cut(Part.makeCylinder(1.65, 7, App.Vector(0, 0, 4.5)))
        arm = Part.makeBox(17, 8, 4, App.Vector(-24, -4, 6))
        arm = arm.fuse(Part.makeCylinder(4, 4, App.Vector(-24, 0, 6)))
        arm = arm.cut(Part.makeCylinder(1.1, 6, App.Vector(-24, 0, 5)))
        pod = pod.fuse(arm)
    else:
        # Four M3 screws hold a fixed rear pod to the chassis.
        for x in (-11, 11):
            for y in (-9, 9):
                pod = pod.cut(Part.makeCylinder(1.65, 7, App.Vector(x, y, 4.5)))
    return pod.removeSplitter()


def n20_motor_local():
    # Reference model from supplied image: 12x10 gearbox, 15 mm motor, 3 mm x 10 mm shaft.
    gearbox = Part.makeBox(12, 9, 10, App.Vector(-6, -10, 13.2))
    motor = Part.makeCylinder(6, 15, App.Vector(0, 14, 18.2), App.Vector(0, -1, 0))
    shaft = Part.makeCylinder(1.5, 10, App.Vector(0, -20, 18.2), App.Vector(0, 1, 0))
    return gearbox.fuse(motor).fuse(shaft)


def wheel_local():
    tire = Part.makeCylinder(19, 14, App.Vector(0, -7, 0), App.Vector(0, 1, 0))
    bore = Part.makeCylinder(1.65, 14, App.Vector(0, -7, 0), App.Vector(0, 1, 0))
    recess = Part.makeCylinder(13, 10, App.Vector(0, -5, 0), App.Vector(0, 1, 0))
    hub = Part.makeCylinder(5.5, 10, App.Vector(0, -5, 0), App.Vector(0, 1, 0))
    return tire.cut(bore).cut(recess.cut(hub)).removeSplitter()


# Main chassis, kept flat for support-free printing.
chassis = rounded_plate(180, 100, 10, 4)

# Front kingpins (M3) at +/- 43 mm track position.
for y in (-43, 43):
    chassis = chassis.cut(Part.makeCylinder(1.65, 8, App.Vector(62, y, -2)))
    # Wide washer recess underneath.
    chassis = chassis.cut(Part.makeCylinder(4.2, 1.2, App.Vector(62, y, -0.1)))

# Rear fixed motor-pod screw pattern.
for y0 in (-43, 43):
    for dx in (-11, 11):
        for dy in (-9, 9):
            chassis = chassis.cut(Part.makeCylinder(1.65, 8, App.Vector(-62 + dx, y0 + dy, -2)))

# SG90-class servo area: body opening and two adjustable ear slots.
servo_opening = Part.makeBox(24, 13.5, 8, App.Vector(28, -6.75, -2))
chassis = chassis.cut(servo_opening)
for x in (25, 55):
    slot = Part.makeBox(8, 3.4, 8, App.Vector(x - 4, -1.7, -2))
    slot = slot.fuse(Part.makeCylinder(1.7, 8, App.Vector(x - 4, 0, -2)))
    slot = slot.fuse(Part.makeCylinder(1.7, 8, App.Vector(x + 4, 0, -2)))
    chassis = chassis.cut(slot)

# Battery tray and two strap slots.
for wall in (
    Part.makeBox(67, 2.5, 9, App.Vector(-38, -23, 4)),
    Part.makeBox(67, 2.5, 9, App.Vector(-38, 20.5, 4)),
    Part.makeBox(2.5, 46, 9, App.Vector(-38, -23, 4)),
    Part.makeBox(2.5, 46, 9, App.Vector(26.5, -23, 4)),
):
    chassis = chassis.fuse(wall)
for x in (-25, 13):
    for y in (-30, 24):
        chassis = chassis.cut(Part.makeBox(11, 6, 8, App.Vector(x - 5.5, y, -2)))

# Bumpers.
for x in (-91, 91):
    bumper = rounded_plate(10, 82, 4, 7)
    bumper.translate(App.Vector(x, 0, 0))
    chassis = chassis.fuse(bumper)
chassis = chassis.removeSplitter()
chassis_obj = feature("ChassisV2", "01 Chassis V2 - print 1", chassis, (0.82, 0.18, 0.12))

# Four motor pods.
left_steer = motor_pod_local(True)
left_steer.translate(App.Vector(62, -43, 0))
right_steer = mirror_y(motor_pod_local(True))
right_steer.translate(App.Vector(62, 43, 0))
left_rear = motor_pod_local(False)
left_rear.translate(App.Vector(-62, -43, 0))
right_rear = mirror_y(motor_pod_local(False))
right_rear.translate(App.Vector(-62, 43, 0))

pod_specs = (
    ("FrontLeftSteeringPod", "02 Front left steering motor pod", left_steer),
    ("FrontRightSteeringPod", "03 Front right steering motor pod", right_steer),
    ("RearLeftMotorPod", "04 Rear left fixed motor pod", left_rear),
    ("RearRightMotorPod", "05 Rear right fixed motor pod", right_rear),
)
pod_objs = [feature(name, label, shape, (0.18, 0.32, 0.78)) for name, label, shape in pod_specs]

# Reference motors and wheels in assembly positions.
reference_objs = []
for axle_x, side_y, prefix in (
    (62, -43, "FrontLeft"), (62, 43, "FrontRight"),
    (-62, -43, "RearLeft"), (-62, 43, "RearRight"),
):
    motor_shape = n20_motor_local()
    wheel_shape = wheel_local()
    if side_y > 0:
        motor_shape = mirror_y(motor_shape)
        wheel_shape = mirror_y(wheel_shape)
    motor_shape.translate(App.Vector(axle_x, side_y, 0))
    # Wheel centre is 25 mm outward from the motor-pod centre.
    wheel_shape.translate(App.Vector(axle_x, side_y + (-25 if side_y < 0 else 25), 18.2))
    motor_obj = feature(prefix + "MotorRef", prefix + " N20 motor - reference", motor_shape, (0.72, 0.72, 0.74))
    wheel_obj = feature(prefix + "Wheel", prefix + " wheel - print", wheel_shape, (0.10, 0.10, 0.10))
    if motor_obj.ViewObject:
        motor_obj.ViewObject.Transparency = 15
    reference_objs.extend((motor_obj, wheel_obj))

# Tie rod and servo horn are reference geometry for 2 mm rod/ball links.
tie_rod = Part.makeCylinder(1.0, 86, App.Vector(38, -43, 9), App.Vector(0, 1, 0))
tie_obj = feature("TieRodRef", "Front tie rod - 2 mm rod reference", tie_rod, (0.92, 0.72, 0.12))
servo_horn = Part.makeBox(28, 3, 3, App.Vector(35, -1.5, 8))
servo_horn = servo_horn.cut(Part.makeCylinder(1.1, 5, App.Vector(60, 0, 7)))
horn_obj = feature("ServoHornRef", "SG90 servo horn/link reference", servo_horn, (0.92, 0.72, 0.12))

assembly = doc.addObject("App::DocumentObjectGroup", "AssemblyV2")
assembly.Label = "4 Motor RC Car with Front Steering"
for obj in [chassis_obj] + pod_objs + reference_objs + [tie_obj, horn_obj]:
    assembly.addObject(obj)

doc.recompute()
fcstd = os.path.join(OUT, "RC_Car_4Motor_Steering_V2.FCStd")
doc.saveAs(fcstd)

# Export each printable component at the origin/orientation used for modelling.
def export_shape(name, shape):
    temp = doc.addObject("Part::Feature", "ExportTemp")
    temp.Shape = shape
    doc.recompute()
    Mesh.export([temp], os.path.join(STL, name))
    doc.removeObject("ExportTemp")


export_shape("chassis_v2.stl", chassis)
export_shape("front_left_steering_pod.stl", motor_pod_local(True))
export_shape("front_right_steering_pod.stl", mirror_y(motor_pod_local(True)))
export_shape("rear_left_motor_pod.stl", motor_pod_local(False))
export_shape("rear_right_motor_pod.stl", mirror_y(motor_pod_local(False)))

# Flat wheel: cylinder axis along Z.
wheel_z = Part.makeCylinder(19, 14)
wheel_z = wheel_z.cut(Part.makeCylinder(1.65, 14))
recess_z = Part.makeCylinder(13, 10, App.Vector(0, 0, 2))
hub_z = Part.makeCylinder(5.5, 10, App.Vector(0, 0, 2))
wheel_z = wheel_z.cut(recess_z.cut(hub_z)).removeSplitter()
export_shape("wheel_38x14_print_4.stl", wheel_z)

doc.recompute()
doc.save()

checks = [
    ("chassis", chassis),
    ("front_pod", motor_pod_local(True)),
    ("rear_pod", motor_pod_local(False)),
    ("wheel", wheel_z),
]
print("CREATED", fcstd)
for name, shape in checks:
    print(name, "valid=", shape.isValid(), "solids=", len(shape.Solids))
