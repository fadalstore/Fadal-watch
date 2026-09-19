import { Router, type IRouter } from "express";
import healthRouter from "./health";
import guardianRouter from "./guardian";

const router: IRouter = Router();

router.use(healthRouter);
router.use(guardianRouter);

export default router;
